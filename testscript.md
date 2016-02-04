Instructions on using testscrip
#################################

testsctip1

This bash scrips executes ./griddle for a specific accuracy and sepcific problem
set in config.json
Comman line arguments required:
1. maximum error: ex 0.0000001
NOTE: must include 0.
2. problem number: ex p0 or p1

sample execution:
./testscript 0.0001 p0

NOTE: testscript calls plotscrip

#################################

testscript2

This bash scripts functions exaclty like testscript1 but also changes
the inner and outer radius variables within the config.json file in 
addition to the accuracy

p0 corresponds to the image prob0MR.png

p1 corresponds to the image prob1.png

