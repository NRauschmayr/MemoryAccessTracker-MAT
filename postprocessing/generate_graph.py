import numpy
import pydot
import sys
import matplotlib.pyplot as plt
from matplotlib import offsetbox
import matplotlib.colors as colors

class Graph(object):
  
   def __init__(self, filename):

      self.filename = filename
      # dictionaries to keep track of edges, nodes and their counters
      self.tree        = {}
      self.edges       = {}
      self.graph       = None
      # read output of PIN tool
      data             = self.prepare_input() # postprocess output file. Compute strides and assign blockIDs.
      self.ips	       = data[:,0]
      self.blockIds    = data[:,7]
      self.strides     = data[:,8]
      self.objectSizes = data[:,6]
      self.isRead      = data[:,4]
      self.total       = data.shape[0]
      self.sourcelines = {}

      # Read sourcelines for ip
      f = open("sourcelines.txt")
      for i in f.readlines():
        tmp  = i.split()
        line = tmp[1].split("/")[-1]
        self.sourcelines[int(tmp[0])] = line.replace(":","-")

   def create_graph(self):
     
     # Create pydot Graph and root node
     self.graph      = pydot.Dot(graph_type='digraph')
     previous_node   = pydot.Node("Root")

     parent_key      = "Root"
     grandparent_key = "RootParent"
     self.tree = {parent_key: [previous_node, 1, 0]}

     # Lookup sourcecode line
     for ip, blockId, stride, objectSize, isRead  in zip( self.ips, self.blockIds, self.strides, self.objectSizes, self.isRead ):
       sourceline = " "
       try:
	  sourceline = self.sourcelines[ip]
       except:
          pass
       # key for each node in the graph is blockID, stride, size and sourceline 
       key = (blockId, stride, objectSize) #, sourceline) 

       # check if node already exists        
       if key not in self.tree:

         # create a new node and edge
         node = pydot.Node(str(blockId) + "_" + str(stride) + "_" + str(objectSize) ) #+ "_" + str(sourceline)) 
         self.graph.add_node(node)
         new_edge = pydot.Edge(previous_node, node)
         self.graph.add_edge( new_edge )

         # add new node with node counter 1 to dictionary          
         self.tree[key]  = [node, 1, isRead] 

         # add new edge with edge counter 1 to dictionary
         self.edges[key] = {parent_key: {grandparent_key: [new_edge, 1] } , "Last": parent_key}

         # remember keys for next iteration
         grandparent_key    = parent_key
         parent_key         = key
         previous_node      = node

       #key already exist in Graph, so increase counter
       else:

         # edge to parent does not exist
         if (parent_key not in self.edges[key].keys()):
           new_edge = pydot.Edge(previous_node, self.tree[key][0]) 
           self.graph.add_edge( new_edge )
           self.edges[key].update({parent_key: { grandparent_key: [new_edge, 0] }})

         # edge to grandparent does not exist in dictionary
         if grandparent_key not in self.edges[key][parent_key].keys():
           k = self.edges[key][parent_key].keys()[0]
           new_edge = self.edges[key][parent_key][k][0]
           self.edges[key][parent_key].update({grandparent_key: [new_edge,0]})

         # increase edge_counter and update dictionaries 
         edge_counter = self.edges[key][parent_key][grandparent_key][1]
         self.edges[key][parent_key][grandparent_key][1] = edge_counter + 1
         self.edges[key]["Last"] = parent_key
         grandparent_key = self.edges[key]["Last"]

         # increase node counter
         previous_node, counter, isRead = self.tree[key]
         parent_key = key
         self.tree[parent_key] = [previous_node, counter + 1, isRead]

     # set edge labels and mark most visited nodes (larger size)
     for key in self.tree:
        node, counter, isRead = self.tree[key]
        node_label = node.get_name() + " - " + str(counter) 
        node.set_label( node_label )

        try:
          for parent_key in self.edges[key].keys():
            if parent_key != "Last":
              edge_label = ""
              for grandparent_key in self.edges[key][parent].keys():
                if grandparent_key != "Last":
                  edge_counter = self.edges[key][parent_key][grandparent_key][1]
                  edge_label =  edge_label + "\n" + str(edge_counter) + " " + str(grandparent_key)
              self.edges[key][parent_key][grandparent_key][0].set_label( edge_label )
        except:
          pass
        
        if counter > 0.001 * self.total:
          node.set_fontsize(24)

        #mark write accesses 
        if not isRead:
            node.set_style("filled, bold")          

   def get_ngrams(self):

     ngrams = {}
     nodeList = []
     isReadList = []

     grandparent = "RootParent"
     parent = "Root"

     for i in range( 0, self.total ):

       key = ( self.blockIds[i] , self.strides[i] , self.objectSizes[i] , self.sourcelines[self.ips[i]] )
       for current in self.tree.keys():
         
         if key[0] == current[0] and key[1] == current[1]  and key[2] == current[2]  and current in self.edges_reduced.keys(): #and key[3] == current[3] 
           if parent in self.edges_reduced[ current ] and parent != "Root":
             if grandparent in self.edges_reduced[ current ][ parent ]:
               if len( nodeList ) == 0:
                 nodeList.append(( parent[0], parent[1], parent[2] )) #, parent[3] ))
                 isReadList.append( self.isRead[i-1] )
               nodeList.append( key )
               isReadList.append( self.isRead[i] )

               # if current node and it's parent node is already in nodeList, then one ngram is found
               if len(nodeList) > 2 and key[0] == nodeList[1][0] and key[1] == nodeList[1][1] and key[2] == nodeList[1][2] and parent[0] == nodeList[0][0] and parent[1] == nodeList[0][1]:
                 # create ngram_key  
                 parent_tup = ( parent[0] , parent[1] , parent[2] ) #, parent[3] )  
                 ngram_key = parent_tup + key
                 # check if an ngram with this key already exists
                 if ngram_key in ngrams:

                   # Ngrams can start with the same blockIDs, strides. As such an ngram_key in the ngrams-dict can have mutliple ngrams
                   append = True
                   for counter in range(0, len( ngrams[ ngram_key ][ 0 ] )):
                       # check if this specific ngram has already been seen: if yes increase counter by one. if not add a new list to the existing key in ngrams_dict 
                      if nodeList == ngrams[ ngram_key ][ 0 ][ counter ]:
                        append = False
                        ngrams[ ngram_key ][ 2 ][ counter ] = ngrams[ ngram_key ][ 2 ][ counter ] + 1
                        break
                   # ngram_key already exist, but ngram itself does not, so extend the list  
                   if append:
                      #ngrams contains following list [nodeList (blockIDs, strides, objectSizes)] [number of occurences] [accesstypes] 
                      ngrams[ ngram_key ][0].append( nodeList )
                      ngrams[ ngram_key ][1].append( isReadList )
                      ngrams[ ngram_key ][2].append( 1 )# how often did the ngram appear
                 # this ngram has not been so far, so store it in the ngrams-dict with counter 1
                 else:
                   ngrams[ngram_key] = [ [nodeList] , [isReadList] , [1] ]

                 nodeList   = [ parent_tup, key ]
                 isReadList = [ self.isRead[i-1], self.isRead[i] ]
                 ips        = [ self.ips[i-1], self.ips[i] ]
                 break

           #reset lists
           else:
               nodeList   = [] 
               isReadList = []
           break
       grandparent = parent
       parent = current

     # if the end of the memory graph is reached and nodeList is not empty
     if len( nodeList ) > 0 and len( nodeList ) > 0.5 * self.total:
       if nodeList[0] in ngrams: 
        ngrams[ nodeList[0] + nodeList[1] ][0].append( nodeList )
        ngrams[ nodeList[0] + nodeList[1] ][1].append( isReadList )        
        ngrams[ nodeList[0] + nodeList[1] ][2].append( 1 )
       else:
        ngrams[ nodeList[0] + nodeList[1] ] = [ [nodeList] , [isReadList] , [1] ]

     occurences = numpy.array([])

     counter = 0    
     for key in ngrams:
       for entry in range(0, len(ngrams[key][2])):
         # compute how often the ngram appears and only write out the ones which appear relativel often
         ratio = ngrams[key][2][entry]/float(self.total)
         if ratio > 0.01:
           f = open(sys.argv[1] + ".ngram" + str(counter), "w")
           for length in range(0, len(ngrams[key][0][entry])):
             f.write("%s %s %s\n" %(ngrams[key][0][entry][length][0], ngrams[key][0][entry][length][1], ngrams[key][1][entry][length]))
           occurences = numpy.append(occurences, ngrams[key][2][entry])
           f.close()  
           counter = counter + 1
     # store 
     numpy.savetxt(sys.argv[1]+".n", occurences, fmt="%lf")

   def plot_graph(self, outfilename):
      if self.graph == None:
        print "Run first create_graph()"
        return False
      self.graph.write_png(outfilename+".png")

   def reduce_graph(self, threshold=0):

      if len(self.edges.keys()) == 0:
        print "Run first create_graph()"
        return False

      self.edges_reduced = {}
      self.graph = pydot.Dot(graph_type='digraph')

      # iterate through nodes and edges and filter by value
      for key in self.edges.keys():

        # check for most visited nodes
        if self.tree[key][1] > threshold * self.total:

          # iterate over edges and connect most visited nodes
          for parent_key in self.edges[key]:

            if parent_key != "Last" and self.tree[parent_key][1] > threshold * self.total:

              edge_label = ""

              for grandparent_key in self.edges[key][parent_key].keys():

                n = self.edges[key][parent_key][grandparent_key]

                if self.tree[parent_key][1] > threshold * self.total and self.edges[key][parent_key][grandparent_key][1] > threshold * self.total:
                  if key not in self.edges_reduced:
                    self.edges_reduced[key] = { parent_key: { grandparent_key: n } }
                  if parent_key not in self.edges_reduced[key]:
                    self.edges_reduced[key].update({ parent_key: { grandparent_key: n } })
                  if grandparent_key not in self.edges_reduced[key][parent_key]:
                     self.edges_reduced[key][parent_key].update({ grandparent_key: n })
                  else:
                     self.edges_reduced[key][parent_key][grandparent_key] = n
                  edge_counter = self.edges[key][parent_key][grandparent_key][1]
                  edge_label =  edge_label + "\n" + str(edge_counter) + " " + str(grandparent_key[0]) + "_" + str(grandparent_key[1]) + "_" + str(grandparent_key[2])

              if edge_label != "":
                new_edge =  pydot.Edge(self.tree[parent_key][0], self.tree[key][0])
                self.graph.add_node(self.tree[parent_key][0])
                self.graph.add_node(self.tree[key][0])
                self.graph.add_edge( new_edge )
                new_edge.set_label( edge_label ) 


   def prepare_input(self):

      data = numpy.loadtxt(self.filename, dtype=numpy.int64)

      if data.shape[1] < 8:
        block_ids     = numpy.zeros(data.shape[0])
        stride        = numpy.zeros(data.shape[0])
        unique_blocks = numpy.unique(data[:,3])

        counter = 0

        #iterate over unique memory blocks
        for i in unique_blocks:

          #Get the entries that access the same memory block  
          inds = numpy.where(data[:,3] == i)

          #assign each memory block a specific ID
          block_ids[inds] = counter

          #compute the delta offsets
          n  = len(inds[0])
          z  = numpy.zeros(n)
          t1 = data[inds,2]
          t2 = data[inds,2]
          z[0:n-1] = t2[0,1:n]-t1[0,0:n-1]
          # reset last value
          z[n-1] = z[n-2]
          stride[inds] =  z
          counter = counter + 1

        stride.shape = (stride.shape[0],1)
        block_ids.shape = (block_ids.shape[0], 1)
        data = numpy.hstack([data, block_ids , stride])
        numpy.savetxt(self.filename, data, fmt="%d %d %d %d %d %d %d %d %d")

      return data 
   
if __name__ == "__main__":
   g = Graph(sys.argv[1])
   g.create_graph()
   g.plot_graph(sys.argv[1])
   g.reduce_graph(threshold=0.001)
   g.plot_graph(sys.argv[1]+"_reduced")
   g.get_ngrams()
