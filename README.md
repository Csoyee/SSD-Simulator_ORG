# SSD-Simulator
SSD simulator using blktrace

## USAGE
```bash
$> make
$> ./ftl -s [logical ssd size (unit: GB)] -f [log filename] # option -s, -f is necessary

```

## Optional Option
- `-r [file name] : printBlkStat to file (main.c)`  
- `-m [stream num] : specifies the number of streams (default: 1,maximum: 4)`
- `-o [op region] : specifies the op region (default: 10)`


## blktrace
1. ssd partitioning
- make partition of ssd that fits to the size of the simulator.

2. get block trace
```bash
$> sudo btrace [device name] | grep "D\s\+W" 
```

## Multi-Stream Feature
- [commit log](https://github.com/Csoyee/SSD-Simulator_ORG/commit/db259024f91bb142dc3a98ee503fb22372af0eac)
