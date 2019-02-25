import glob
import csv
import os
import statistics

## reset file
with open("tables.csv", 'w', newline='') as myfile:
    wr = csv.writer(myfile, quoting=csv.QUOTE_ALL)
    wr.writerow("")
    wr.writerow("")

# all files except starting with t (no tables)
fileList = glob.glob("[!t]*.csv") 

testSet = set()

for file in fileList:
    parts = file.split("_TN")
    testSet.add(parts[0])


for test in testSet:
    GPU = test.split("_")[0]

    testFiles = glob.glob(test + "*.csv")
    AverageData = []
    for testString in testFiles :
            print ("Calculating: " + testString)
            data = []
            header = []
            hCount = 0
            
            with open(testString, 'r') as csvfile:
                    reader = csv.reader(csvfile, delimiter=',')
                    for row in reader:
                            if (hCount <3):
                                    header.append(row)
                            else :
                                    data.append(row)
                            hCount = hCount + 1

            #vars for average lists
            computeStart = 0
            computeEnd = 0
            graphicsStart = 0
            graphicsEnd = 0
            diff = 0

            totalFrames = 0
            frameTimes = []
            cTimes = []
            gTimes = []


            averageDifference = []

            for ind in range(0, len(data), 1) :
                    d = data[ind]
                    totalFrames = int(d[0])
                    frameTimes.append(float(d[1]))
                    cTimes.append(float(d[4]))
                    gTimes.append(float(d[7]))
                    computeStart = float(d[2])
                    computeEnd = float(d[3])
                    graphicsStart = float(d[5])
                    graphicsEnd = float(d[6])

                    if (computeStart < graphicsStart):
                            diff = computeEnd - graphicsStart
                    elif (computeStart > graphicsStart) :
                            diff = graphicsEnd - computeStart

                                                           
                    averageDifference.append(diff)
                    


            # calculate averages and std dev
            AverageData.append([totalFrames, statistics.mean(frameTimes), statistics.stdev(frameTimes), statistics.variance(frameTimes), statistics.mean(cTimes), statistics.stdev(cTimes), statistics.variance(cTimes), statistics.mean(gTimes), statistics.stdev(gTimes), statistics.variance(gTimes), statistics.mean(averageDifference), statistics.stdev(averageDifference), statistics.variance(averageDifference)])


    # save results to file
    with open("tables.csv", 'a', newline='') as myfile:
            wr = csv.writer(myfile, quoting=csv.QUOTE_ALL)
            wr.writerow(GPU)
            wr.writerow(header[0])
            wr.writerow(header[1])
            wr.writerow(["Total Frames", "Mean FrameTime", "STDev", "Variance", "Mean Compute Time", "STDev", "Variance", "Mean Graphics Time", "Stdev", "Variance", "Mean Difference", "STDev", "Variance"])
            wr.writerows(AverageData)
            wr.writerow("")
            wr.writerow("")
            wr.writerow("")
