# Local maximum peak detection

Isolated hardware module that detects peaks by flagging samples whose nearest neighbors in time are less than it. 

Accompanied by testbench file that takes in input file and outputs results to output. 

### Input file format: 
Time (in steps of 10ns), Detector Waveform (voltage reading in volts scaled by 10e6), Scan waveform (voltage reading in volts scaled by 10e6) 

NOTE: Scan waveform is not used here, so you can safely put 0s in the third column of the input file. 


### Output file format: 
Sample number, voltage of detector at sample (actual voltage reading in volts scaled by 10e6)

NOTE: Sample number is calculated by taking the time and dividing by 10, with assumption that each time step in input file are in steps of 10ns