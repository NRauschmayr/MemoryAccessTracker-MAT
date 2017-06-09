import matplotlib.pyplot as plt
import numpy
import os
import copy
import itertools

# compute modified Levensthein distance
def computeLevenhsteinDistance(b1,o1,b2,o2,isRead1,isRead2):

   counter = 0 
   distance = 0 

   for i in itertools.izip_longest(b1,o1,b2,o2,isRead1,isRead2):
      # check if read/write matches
      if i[4] == i[5]:
        #check if block IDs match
        if i[0] != i[2]:
           distance = distance + 1
        #check if offsets match
        if i[1] != i[3]:
           distance = distance + 1
      else:
        distance = distance + 2 
      counter = counter + 1
   # return normalized distance 
   return distance/float(counter*2)

# mix block IDs. Assign block IDs from Ngram1 to Ngram2
def mix(b1, b2, o1, o2, isRead1, isRead2):

  c = 10

  min = len(b1)+len(b2)+len(o1)+len(o2)

  counter2 = 0
  a = numpy.array([]) 
  org_b2 = copy.deepcopy(b2)
  org_o2 = copy.deepcopy(o2)
  org_isRead2 = copy.deepcopy(isRead2)
  prev_inds = numpy.array([])
  _, idx1 = numpy.unique(b1, return_index=True)

  # iterate over unique blocks IDs in ngram1
  for i in numpy.unique(b1):
    a = numpy.append(a, b1[numpy.sort(idx1)]) 
    counter = 0
    b2 = copy.deepcopy(org_b2)
    _, idx2 = numpy.unique(b2, return_index=True)

    for l in b2[numpy.sort(idx2)]:
      inds = numpy.where(b2 == l)[0]
      b2[inds] = a[counter+counter2]
      prev_inds = inds
      counter = counter + 1
    counter2 = counter2 + 1 
    pos = -1
   
    # Sequence alignment. If Ngram2 is shorter shift it by one position and
    # recompute Levenhstein Distance  
    for x in range(0,numpy.abs(b1.shape[0]-b2.shape[0])+1): 
       if b1.shape[0] > b2.shape[0] and pos > -1:
         b2 = numpy.insert(b2,pos,None)
         o2 = numpy.insert(o2,pos,None)
         isRead2 =  numpy.insert(isRead2,pos,None)

       pos = pos + 1
       result = computeLevenhsteinDistance(b1,o1,b2,o2,isRead1,isRead2)
       
       if result < min:
         min = result

    o2 = copy.deepcopy(org_o2)
    isRead2 = copy.deepcopy(org_isRead2)
    result = computeLevenhsteinDistance(b1,o1,b2,o2,isRead1,isRead2)

    if result < min:
      min = result

  return min     

files = [f for f in os.listdir('.')]
files = sorted(files)
labels = []
data = numpy.array([])

nGramList = []
counter = 1
labels = []
c = 10

for i in sorted(files):

  if "ngram" in i: 
    tmp     = numpy.loadtxt(i)
    try:
      tmp.shape[1]
    except:
      tmp.shape = (1,tmp.shape[0])
    blocks  = tmp[:,0] 
    blocks_tmp = copy.deepcopy(blocks)

    # Reassign unique block IDs for each Ngram in the set of Ngrams
    for x in numpy.unique(blocks_tmp):
      inds = numpy.where(blocks_tmp == x)
      blocks[inds] = c 
      c = c + 1

    offsets = tmp[:,1] 
    isRead  = tmp[:,2]

    nGramList.append([blocks,offsets,i,isRead])
    labels.append(i)
    previous = i

  counter = counter + 1

results = numpy.array([])

counter_i = 1 
counter_l = 0

#iterate over ngrams
for i in nGramList:

    counter_l = 0

    for l in nGramList:
      # compare different ngrams
      if i[2] != l[2]:
        # check which ngrams is larger
        if len(numpy.unique(i[0])) >= len(numpy.unique(l[0])):
          result = mix(i[0],l[0],i[1],l[1],i[3],l[3])
        else:
          result = mix(l[0],i[0],l[1],i[1],l[3],i[3])
      # same ngram
      else:
        result = 0.0 
      results = numpy.append(results, result)
      counter_l = counter_l + 1
    counter_i = counter_i + 1


# plot similarity matrix
results.shape = (results.shape[0]/len(labels),len(labels))
import matplotlib.pyplot as plt
fig, ax = plt.subplots()
heatmap = ax.pcolor(results)
for x in range(0,results.shape[1]):
  for y in range(0,results.shape[1]): 
    plt.text(x + 0.5, y + 0.5, '%.2f' % results[y, x],
             horizontalalignment='center',
             verticalalignment='center',
             )
ax.set_yticks(numpy.arange(results.shape[0])+0.5, minor=False)
ax.set_xticks(numpy.arange(results.shape[1])+0.5, minor=False)
plt.tight_layout()
plt.gcf().subplots_adjust(bottom=0.35,left=0.35)
plt.axis([0, results.shape[1], 0, results.shape[1]])
plt.xticks(rotation=90)
ax.set_xticklabels(labels, minor=False)
ax.set_yticklabels(labels, minor=False)
plt.show()      

