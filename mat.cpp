#include <signal.h>
#include <map>
#include <unistd.h>
#include "splay-tree.h"
#include <set>

#include "pin.H"
#include "instlib.H"
#include "portability.H"

//define allocation functions
#define MMAP 	"mmap"
#define REALLOC "realloc"
#define CALLOC 	"calloc"
#define MALLOC 	"malloc"
#define SBRK 	"sbrk"
#define BRK     "brk"
#define FREE 	"free"
#define MUNMAP  "munmap"
#define POSIX_MEMALIGN "posix_memalign"

static std::map<ADDRINT,ADDRINT> allocations;
static unsigned int threshold = 0;
static std::map<ADDRINT, int> ip_map;
static std::map<ADDRINT, std::string> sourcelines;

using namespace INSTLIB;
FILTER filter;

BUFFER_ID bufId;
PIN_LOCK fileLock;
TLS_KEY buf_key;

BOOL EnableInstrumentation = TRUE;

FILE *traceFile;
FILE *ipFile;

static splay_tree  tree = splay_tree_new((splay_tree_compare_fn) splay_tree_compare_ints,
                                   0,0);

//SignalHandler do enable instrumentation for memory accesses
BOOL SignalHandler1(THREADID, INT32, CONTEXT *, BOOL, const EXCEPTION_INFO *, void *){
  std::cout << "Instrumenation enabled" << std::endl;
  EnableInstrumentation = TRUE;
//  PIN_RemoveInstrumentation();
  return FALSE;
}

//SignalerHandler do disable instrumentation for memory accesses
//We cannot disable instrumentation for allocation functions, otherwise we loose
//important information
BOOL SignalHandler2(THREADID, INT32, CONTEXT *, BOOL, const EXCEPTION_INFO *, void *){
  fprintf(traceFile, "0 0\n");
  std::cout << "Instrumenation disabled" << std::endl;
  EnableInstrumentation = FALSE;
//  PIN_RemoveInstrumentation();
  return FALSE;
}

//Record data entries into file TODO: compression and binary output for optimization
static std::map<ADDRINT, std::string> string_of_instructions;
VOID Record(ADDRINT ip, ADDRINT ea, UINT32 size, BOOL type)
{
   //Get sourceline of the corresponding ip
   auto it = ip_map.insert(std::make_pair(ip,1));
   if (it.second){
       //string_of_instructions[addr] = INS_Disassemble(addr);
       std::map<ADDRINT,std::string>::const_iterator it = sourcelines.find(ip);

      //Get sourceline of the corresponding ip
      std::string filename;
      INT32 line = 0;
      PIN_LockClient();
      PIN_GetSourceLocation(ip, NULL, &line, &filename);
      PIN_UnlockClient();
      std::stringstream ss;
      ss << filename << ":" << line;
      sourcelines[ip] = ss.str();
   }

   splay_tree_node n = splay_tree_lookup(tree, ea);
   uint64_t tmp = -1;

   if( n != 0){
     tmp = ea - n->key;
     fprintf(traceFile, "%lu %lu %lu %lu %d\n", (long unsigned) ip, (long unsigned) ea, tmp, n->key, type);
    } 
   else //if we cannot find the corresponding allocation to which the memory access belongs
      fprintf(traceFile, "%lu %lu -1 -1 %d\n", (long unsigned) ip, (long unsigned) ea, type);
}

//Instrument Load and Stores and obtain information about
//ip, address of memory access and size of load (1)/store (0)  
//We ignore memory accesses to the stack for the time being
BOOL InstrumentMemAccess (INS ins){

 UINT32 memOperands = INS_MemoryOperandCount(ins);
 for (UINT32 memOp = 0; memOp < memOperands; memOp++){

 if (INS_MemoryOperandIsRead(ins, memOp) && !INS_IsStackRead(ins))
    INS_InsertPredicatedCall(
        ins, IPOINT_BEFORE, (AFUNPTR)Record,
        IARG_INST_PTR, 
        IARG_MEMORYREAD_EA, 
        IARG_MEMORYREAD_SIZE, 
        IARG_BOOL, 1,   
        IARG_END);

 if (INS_MemoryOperandIsWritten(ins, memOp) && !INS_IsStackWrite(ins))
    INS_InsertPredicatedCall(
        ins, IPOINT_BEFORE, (AFUNPTR)Record, 
        IARG_INST_PTR, 
        IARG_MEMORYWRITE_EA, 
        IARG_MEMORYWRITE_SIZE,  
        IARG_BOOL, 0,    
        IARG_END);
 } 
  return (TRUE);
}

//Wrapper for allocation functions: We need to obtain function entry arguments and
//function exit arguments for malloc, realloc, calloc, brk, sbrk, mmap, munmap,
//free, posix_memalign, memalign
static size_t malloc_size = 0;
VOID alloc_before(size_t size){
   if (size > threshold)
     malloc_size = size;
}

VOID alloc_after(ADDRINT addr){
   if( malloc_size > threshold) 
     splay_tree_insert(tree, (splay_tree_key) addr, (splay_tree_value) addr+malloc_size);
}

static intptr_t sbrk_size = 0;
VOID sbrk_before(intptr_t size){ 
     sbrk_size = size;
}

VOID sbrk_after(ADDRINT addr){
   //sbrk can decrease the size of the heap
   if( sbrk_size > threshold) 
     splay_tree_insert(tree, (splay_tree_key) addr, (splay_tree_value) addr+sbrk_size);
   else if (sbrk_size < 0)
     splay_tree_remove(tree, addr); 
}

static intptr_t brk_addr1 = 0;
static intptr_t brk_addr2 = 0;
VOID brk_before(ADDRINT addr){
     brk_addr1 = (intptr_t) sbrk(0);
     brk_addr2 = addr;
}

VOID brk_after(int ret){
   if(( ret == 0) && ((brk_addr2-brk_addr1) > threshold))
     splay_tree_insert(tree, (splay_tree_key) brk_addr1, (splay_tree_value) brk_addr2);
}

static intptr_t free_addr;
VOID free_before(ADDRINT addr){ 
     free_addr = addr;
}

VOID free_after(){
    splay_tree_remove(tree, free_addr);
}

static size_t calloc_size = 0;
VOID calloc_before(size_t n, size_t size){
   calloc_size = n*size;
}

VOID calloc_after(ADDRINT addr){
   if( calloc_size > threshold)
     splay_tree_insert(tree, (splay_tree_key) addr, (splay_tree_value) addr+calloc_size);
}

static size_t realloc_size = 0;
static intptr_t realloc_addr = 0;
VOID realloc_before(ADDRINT addr, size_t size){ 
   realloc_size = size;
   realloc_addr = addr; 
}

//treat realloc as free + malloc
VOID realloc_after(ADDRINT addr){
  if(addr > 0)
     splay_tree_remove(tree, realloc_addr); 
  if (realloc_size > threshold)
     splay_tree_insert(tree, (splay_tree_key) addr, (splay_tree_value) addr+realloc_size);
}

static int posix_memalign_size=0;
static uintptr_t posix_memalign_addr=0;
static void** tmp_src;

VOID posix_memalign_before(ADDRINT src, size_t alignment, size_t size){
  posix_memalign_size=size;
  tmp_src = src;
}

VOID posix_memalign_after(int ret){
   posix_memalign_addr = (ADDRINT) *tmp_src;
   if (ret == 0)
     splay_tree_insert(tree,(splay_tree_key) posix_memalign_addr,(splay_tree_value) posix_memalign_addr+posix_memalign_size);
}

static int memalign_size=0;
VOID memalign_before( size_t alignment, size_t size){
   memalign_size=size;
}
 
VOID memalign_after(ADDRINT addr){
  splay_tree_insert(tree,(splay_tree_key) addr,(splay_tree_value) addr+memalign_size);
}

static size_t mmap_size = 0;
VOID mmap_before(ADDRINT addr, size_t size){
   mmap_size = size;
}

VOID mmap_after(ADDRINT addr){ 
    splay_tree_insert(tree,(splay_tree_key) addr,(splay_tree_value) addr+mmap_size);
}

static uintptr_t munmap_addr;
VOID munmap_before(ADDRINT addr){
  munmap_addr = addr;
}

VOID munmap_after(){
    splay_tree_remove(tree, munmap_addr);
}

//Trace Instrumentation
VOID Trace(TRACE trace, VOID * val)
{
    if (!filter.SelectTrace(trace) )
        return;
 
    //Iterate over basic blocks   
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)){  
        //Iterate over instructions within basic block
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
           //Enter Instrumentation if enabled       
           if (EnableInstrumentation){
             RTN rtn = TRACE_Rtn(trace);
             if (RTN_Valid(rtn) ){
               IMG img = SEC_Img(RTN_Sec(rtn));
               if (IMG_Valid(img)){
                 //Instrument functions of interest e.g. main
                      if (RTN_Name(rtn) == "main")
                         InstrumentMemAccess(ins); 
               }
             }
          } }
      }
}

//Define Instrumentation for allocation functions
VOID RtnInsertCall(IMG img, CHAR *funcname){
  RTN rtn = RTN_FindByName(img, funcname);
  if (RTN_Valid(rtn)){

    RTN_Open(rtn);

    if(!strcmp(funcname, MALLOC)){
      RTN_InsertCall( rtn,
        IPOINT_BEFORE,
        (AFUNPTR)alloc_before,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
        IARG_END);
      RTN_InsertCall( rtn,
        IPOINT_AFTER,
        (AFUNPTR)alloc_after,
        IARG_FUNCRET_EXITPOINT_VALUE,
        IARG_END);
    }
    else if (!strcmp(funcname, SBRK)){
      RTN_InsertCall( rtn,
        IPOINT_BEFORE,
        (AFUNPTR)sbrk_before,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
        IARG_END);
      RTN_InsertCall( rtn,
        IPOINT_AFTER,
        (AFUNPTR)sbrk_after,
        IARG_FUNCRET_EXITPOINT_VALUE,
        IARG_END);
    }
   else if (!strcmp(funcname, BRK)){
      RTN_InsertCall( rtn,
        IPOINT_BEFORE,
        (AFUNPTR)brk_before,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
        IARG_END);
      RTN_InsertCall( rtn,
        IPOINT_AFTER,
        (AFUNPTR)brk_after,
        IARG_FUNCRET_EXITPOINT_VALUE,
        IARG_END);
    }
    else if (!strcmp(funcname, CALLOC)){
      RTN_InsertCall( rtn, 
        IPOINT_BEFORE, 
        (AFUNPTR)calloc_before, 
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
        IARG_END);
      RTN_InsertCall( rtn, 
        IPOINT_AFTER,
        (AFUNPTR)calloc_after,
        IARG_FUNCRET_EXITPOINT_VALUE,
        IARG_END);
    }
    else if(!strcmp(funcname, REALLOC)) {
      RTN_InsertCall( rtn, 
        IPOINT_BEFORE, 
        (AFUNPTR)realloc_before,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
        IARG_END);
      RTN_InsertCall( rtn,
        IPOINT_AFTER,
        (AFUNPTR)realloc_after,
        IARG_FUNCRET_EXITPOINT_VALUE,
        IARG_END);
   }
   else if(!strcmp(funcname, POSIX_MEMALIGN)) {
      RTN_InsertCall( rtn,
        IPOINT_BEFORE,
        (AFUNPTR)posix_memalign_before,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
        IARG_END);
      RTN_InsertCall( rtn,
        IPOINT_AFTER,
        (AFUNPTR)posix_memalign_after,
        IARG_FUNCRET_EXITPOINT_VALUE,
        IARG_END);
   }
    else if(!strcmp(funcname, MMAP)) {
      RTN_InsertCall( rtn,
        IPOINT_BEFORE,
        (AFUNPTR)mmap_before,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
        IARG_END);
      RTN_InsertCall( rtn,
        IPOINT_AFTER,
        (AFUNPTR)mmap_after,
        IARG_FUNCRET_EXITPOINT_VALUE,
        IARG_END);
   }
     else if(!strcmp(funcname, MUNMAP)) {
      RTN_InsertCall( rtn,
        IPOINT_BEFORE,
        (AFUNPTR)munmap_before,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
        IARG_END);
      RTN_InsertCall( rtn,
        IPOINT_AFTER,
        (AFUNPTR)munmap_after,
        IARG_END);
    }
 
    else if(!strcmp(funcname, FREE)){
      RTN_InsertCall( rtn,
        IPOINT_BEFORE,
        (AFUNPTR)free_before,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
        IARG_END);
      RTN_InsertCall( rtn,
        IPOINT_AFTER,
        (AFUNPTR)free_after,
        IARG_END);
    }
    RTN_Close(rtn);
   }
}

//Insert instrumented allocation functions when image is loaded
VOID InstrumentMalloc(IMG img, VOID * v ){
   if(IMG_IsMainExecutable(img)) 
   {
	for(SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
    	    const std::basic_string <char> sec_name = SEC_Name(sec);
	    // Record .data, .bss and .rodata to the list of allocations
	    if(!strcmp(sec_name.c_str(), ".bss") || !strcmp(sec_name.c_str(), ".data") || !strcmp(sec_name.c_str(), ".rodata")) {
		ADDRINT addr = SEC_Address(sec);
		USIZE size = SEC_Size(sec); 
                if(size > threshold) 
                    splay_tree_insert(tree, (splay_tree_key) addr,(splay_tree_value)  addr+size);

                }
          }
     }
    RtnInsertCall(img, (CHAR*)SBRK);
    RtnInsertCall(img, (CHAR*)BRK);
    RtnInsertCall(img, (CHAR*)MALLOC);
    RtnInsertCall(img, (CHAR*)FREE);
    RtnInsertCall(img, (CHAR*)MMAP);
    RtnInsertCall(img, (CHAR*)REALLOC);
    RtnInsertCall(img, (CHAR*)CALLOC);
    RtnInsertCall(img, (CHAR*)MUNMAP);
    RtnInsertCall(img, (CHAR*)POSIX_MEMALIGN);
}

//Write out some statistics at the finalization step
VOID Fini(INT32 code, VOID *v)
{
   PIN_GetLock(&fileLock, 1);

   ipFile =fopen("sourcelines.txt","w");

   for(auto it: sourcelines)
       fprintf(ipFile, "%lu %s\n", it.first, it.second.c_str());
   fflush(ipFile);
   fclose(ipFile);
   PIN_ReleaseLock(&fileLock);

   fclose(traceFile);
   PIN_ReleaseLock(&fileLock);
}


int main(int argc, char *argv[])
{
    PIN_Init(argc, argv);

    traceFile = fopen("memtrace.txt", "w");

    PIN_InterceptSignal(SIGUSR1, SignalHandler1, 0);
    PIN_UnblockSignal(SIGUSR1, TRUE);

    PIN_InterceptSignal(SIGUSR2, SignalHandler2, 0);
    PIN_UnblockSignal(SIGUSR2, TRUE);

    PIN_InitLock(&fileLock);

    IMG_AddInstrumentFunction(InstrumentMalloc, NULL);
    TRACE_AddInstrumentFunction(Trace, NULL);
    filter.Activate();
  
    PIN_AddFiniFunction(Fini, 0);

    PIN_StartProgram();
    
    return 0;
}


