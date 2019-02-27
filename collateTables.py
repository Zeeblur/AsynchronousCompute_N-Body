#get averages

import statistics 
import csv
import os
import math

# Sim parameter class

class DataSet(object):

    def __init__(self, myList):
        self.GPU = myList[0]
        self.SimType = myList[1]
        self.Parameters = myList[2]
        self.Header = myList[3]
        # calculate averages
        self.data = []
        for i in range(4, len(myList), 1):
                self.data.append(myList[i])
        self.averageData()

    def __str__(self):
        return str(self.Parameters)

    def averageData(self):
        totalFrames = []
        meanFrameTime = []
        varFrameTime = []
        meanCompTime = []
        varCompTime = []
        meanGfx = []
        varGfxTime = []
        meanDifference = []
        varDifference = []
        
        for d in self.data:
            totalFrames.append(int(d[0]))
            meanFrameTime.append(float(d[1]))
            varFrameTime.append(float(d[3]))
            meanCompTime.append(float(d[4]))
            varCompTime.append(float(d[6]))
            meanGfx.append(float(d[7]))
            varGfxTime.append(float(d[9]))
            meanDifference.append(float(d[10]))
            varDifference.append(float(d[12]))

        meanVarianceFT = statistics.mean(varFrameTime)
        meanVarianceCT = statistics.mean(varCompTime)
        meanVarianceGT = statistics.mean(varGfxTime)
        meanVarianceDf = statistics.mean(varDifference)
        self.averages = [statistics.mean(totalFrames),
                      statistics.mean(meanFrameTime), math.sqrt(meanVarianceFT), meanVarianceFT,
                      statistics.mean(meanCompTime), math.sqrt(meanVarianceCT), meanVarianceCT,
                      statistics.mean(meanGfx), math.sqrt(meanVarianceGT), meanVarianceGT,
                      statistics.mean(meanDifference), math.sqrt(meanVarianceDf), meanVarianceDf]

                
    def setParameters(self):
        self.particleCount = self.Parameters[1]
        self.ssCount = self.Parameters[3]
        self.scale = self.Parameters[7]
        
                
data = []
data.append([])
count = 0

tableFlag = True

# open the tables file and collate the tables into lists
with open("tables.csv", 'r') as csvfile:
    reader = csv.reader(csvfile, delimiter=',')
    for row in reader :            
        if len(row[0]) == 0 and tableFlag:
            print ("Adding New Dataset")
            tableFlag = False
            count = count+1
            data.append([])
        elif len(row[0]) > 0:
           data[count].append(row)
           tableFlag = True
           #print ("adding row")



# data[tableNum][rowindex]
# 0 = GPU
# 1 = SimType
# 2 = Parameters
# 3 = Headers
# 4+ data

# dataSetObjects
dataSets = [] 

for d in data:
    if (len(d) > 0):
        dataSets.append(DataSet(d))
