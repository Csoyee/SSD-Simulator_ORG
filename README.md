# SSD-Simulator
SSD simulator using blktrace


## USAGE
```bash
$> make
$> ./ftl -s [logical ssd size (unit: GB)] -f [log filename]

```

## Option
`-r [file name] : printBlkStat (main.c)`


## blktrace
1. ssd partitioning
- make partition of ssd that fits to the size of the simulator.

2. get block trace
```bash
$> sudo btrace [device name] | grep "D\s\+W" 
```

## TODO
- MS feature will be added
