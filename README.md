# DS1620-Driver

### Build:

```
make
```

### Use:

```
Bash:
cat /sys/sensors/ds1620/temperature

Python:
./temp_reader
```

### Pin configuration:

```
GPIO_48   <=> CLK
GPIO_49   <=> DQ
GPIO_115  <=> RST
```
