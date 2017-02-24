import numpy
import pydot
import sys
import matplotlib.pyplot as plt
from matplotlib import offsetbox
import matplotlib.colors as colors
from sklearn import manifold, datasets, decomposition, ensemble

class Graph(object):
  
   def __init__(self, filename):

      self.filename = filename
      # dictionaries to keep track of edges, nodes and their counters
      self.tree = {}
      self.edges = {}
      self.keyToInt = {}      
      self.adj_matrix = None
      self.graph = None
      self.content = self.prepare_input()

   def create_graph(self):

     # Read the input file
     isRead  = self.content[:,4]
     a = numpy.vstack([self.content[:,5],self.content[:,6],self.content[:,0]]).T
     a = numpy.array(a, dtype=numpy.int32)
     self.total = a.shape[0]  

     sourcelines = {}
     # Read sourcelines for ip
     f = open("sourcelines.txt")
     for i in f.readlines():
       tmp = i.split()
       f   = tmp[1].split("/")[-1]
       sourcelines[int(tmp[0])] = f.replace(":","-")    

     # Create pydot Graph and root node
     self.graph = pydot.Dot(graph_type='digraph')
     previous_n = pydot.Node("Root", style="filled", fillcolor="red")
     previous_k = "Root"
     self.tree = {previous_k: [previous_n, 1, 0]}

     toInt = 0 # counter to map keys to ints
     for i in range(0,a.shape[0]):
       sourceline = " "
       try:
	  sourceline = sourcelines[a[i,2]]
       except:
          pass
       key = str(a[i,0])+"_"+str(a[i,1])+"_"+sourceline
        
       # create a new node
       if key not in self.tree:
         self.keyToInt[key] = toInt
         toInt = toInt + 1
         node = pydot.Node(key)
         self.tree[key] = [node, 1, isRead[i]] 
         self.graph.add_node(node)
         e = pydot.Edge(previous_n,node)
         self.graph.add_edge( e )
         self.edges[key] = {previous_k:[e,1]}
         previous_k = key
         previous_n = node

       #increase counter
       else:
         if (previous_k not in self.edges[key].keys()):
           e = pydot.Edge(previous_n, self.tree[key][0]) 
           self.graph.add_edge( e )
           self.edges[key].update({previous_k:[e,0]})
         edge_counter = self.edges[key][previous_k][1]
         self.edges[key][previous_k][1] = edge_counter + 1
         previous_n, counter, access_type = self.tree[key]
         previous_k = key
         self.tree[previous_k] = [previous_n, counter + 1, access_type]

     # mark the most visited nodes red
     for i in self.tree:
        node, counter, access_type = self.tree[i] 
        node.set_label(i + " - " + str(counter))
        try:
          for l in self.edges[i].keys():
            edge_counter = self.edges[i][l][1]
            self.edges[i][l][0].set_label( edge_counter )
        except:
          pass
        
        if counter > 0.001 * a.shape[0]:
          node.set_fontsize(24)
        if not access_type:
            node.set_style("filled, bold")          

   def plot_graph(self, outfilename):
      if self.graph == None:
        print "Run first create_graph()"
        return False
      self.graph.write_png(outfilename+".png")

   def reduce_graph(self, threshold=0):
      if len(self.edges.keys()) == 0:
        print "Run first create_graph()"
        return False

      self.graph = pydot.Dot(graph_type='digraph')
      for i in self.edges.keys():
        x = self.keyToInt[i]
        if self.tree[i][1] > threshold * self.total:
          node = pydot.Node(i)
          node.set_label(i + " - " + str(self.tree[i][1]))
          access_type = self.tree[i][2]
          if not access_type:
            node.set_style("filled, bold")
          self.graph.add_node(node)
          for l in self.edges[i]:
             if self.tree[l][1] > threshold * self.total:
                e =  pydot.Edge(self.tree[l][0], self.tree[i][0])
                self.graph.add_edge( e )
                edge_counter = self.edges[i][l][1]
                e.set_label( edge_counter )


   def create_adjacency_matrix(self, threshold=0):
      if len(self.edges.keys()) == 0:
        print "Run first create_graph()"
        return False
     
      # create empty matrix
      dim = len(self.tree.keys())
      self.adj_matrix = numpy.zeros((dim,dim))
      self.names = [0]*dim
      for i in self.edges.keys():
        if self.tree[i][1] > threshold * self.total:
          x = self.keyToInt[i]
          self.names[x] = i
          for l in self.edges[i]:
            try:
              y = self.keyToInt[l]
              if self.tree[l][1] > threshold * self.total:
                self.adj_matrix[x,y] = self.tree[l][1]
            except:
              pass

   def plot_adjacency_matrix(self):
       if self.adj_matrix == None:
         print "Run first create_adjacency_matrix()"
         return False

       # create new color map
       oldCmap = plt.get_cmap('hot')
       minval=0
       maxval=0.7
       newCmap = colors.LinearSegmentedColormap.from_list( 'trunc({n},{a:.2f},{b:.2f})'.format(n=oldCmap.name, a=minval, b=maxval), oldCmap(numpy.linspace(minval, maxval, 100)))

       fig, ax1  = plt.subplots(1,1)
       self.adj_matrix = self.adj_matrix.T
       masked=numpy.ma.masked_where(self.adj_matrix == 0, self.adj_matrix)
       plt.imshow(masked,cmap=newCmap, interpolation='nearest')
       x = range(0, len(self.names))
       plt.xticks(x,x)
       plt.yticks(x,x)
       ax1.set_xticklabels(self.names, rotation=90)
       ax1.set_yticklabels(self.names)
       plt.rc('xtick', labelsize=8)
       plt.rc('ytick', labelsize=8)
       plt.grid(True)
       plt.show()

   def get_adjacency_matrix(self):
      return self.adj_matrix
 
   def get_node_number(self):
      return len(self.tree.keys())
  
   def prepare_input(self):
      data                 = numpy.loadtxt(self.filename)
      if data.shape[1] < 7:
        memory_block_ids     = numpy.zeros(data.shape[0])
        delta_offsets        = numpy.zeros(data.shape[0])
        unique_memory_blocks = numpy.unique(data[:,3])

        counter = 0

        #iterate over unique memory blocks
        for i in unique_memory_blocks:

          #Get the entries that access the same memory block  
          inds = numpy.where(data[:,3] == i)
          #assign each memory block a specific ID
          memory_block_ids[inds] = counter

          #compute the delta offsets
          n = len(inds[0])
          z = numpy.zeros(n)
          t1 = data[inds,2]
          t2 = data[inds,2]
          z[0:n-1] = t2[0,1:n]-t1[0,0:n-1]
          delta_offsets[inds] =  z
          counter = counter + 1

        delta_offsets.shape = (delta_offsets.shape[0],1)
        memory_block_ids.shape = (memory_block_ids.shape[0], 1)
        output = numpy.hstack([data[:,0:5], memory_block_ids , delta_offsets])
        numpy.savetxt(self.filename, output, fmt="%d %d %d %d %d %d %d")
        return output
      else:
        return data 

def select_k(spectrum, minimum_energy = 0.9):
    running_total = 0.0
    total = sum(spectrum)
    if total == 0.0:
        return len(spectrum)
    for i in range(len(spectrum)):
        running_total += spectrum[i]
        if running_total / total >= minimum_energy:
            return i + 1
    return len(spectrum)
   
if __name__ == "__main__":
   g = Graph(sys.argv[1])
   g.create_graph()
   g.plot_graph(sys.argv[1])
   g.reduce_graph(threshold=0.001)
   g.plot_graph(sys.argv[1]+"_reduced")
   g.create_adjacency_matrix(threshold=0.001)
   g.plot_adjacency_matrix()
