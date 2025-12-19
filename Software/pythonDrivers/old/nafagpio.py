#!/usr/bin/env python3
# GPIO used PA17

import gpiod
import time


for chip in gpiod.ChipIter():
	print('{} [{}] ({} lines)'.format(chip.name(),
                                          chip.label(),
                                          chip.num_lines()))
	for line in gpiod.LineIter(chip):
	    offset = line.offset()
	    name = line.name()
	    consumer = line.consumer()
	    direction = line.direction()
	    active_state = line.active_state()
	    print("Name'"+line.name()+"'")

	    print('\tline {:>3}: {:>18} {:>12} {:>8} {:>10}'.format(
	            offset,
	            'unnamed' if name is None else name,
	            'unused' if consumer is None else consumer,
	            'input' if direction == gpiod.Line.DIRECTION_INPUT else 'output',
	            'active-low' if active_state == gpiod.Line.ACTIVE_LOW else 'active-high'))

	
	#chip.close()
		
chip=gpiod.Chip('gpiochip4')
#chip=gpiod.Chip('gpiochip1')
#listgpio=(22, 23, 17, 18 )
#listpin=(15, 16, 11, 12 )
listgpio=(26, 20, 21 )
listpin=(37, 38, 40 )
list=listpin
listlines=[]
for i in list:
	gpio="PIN{:d}".format(i)
	print("Opening Gpio:", gpio)
	line = gpiod.find_line(gpio)
	if line == None:
		gpio="PIN{:d}".format(i)
		print("Opening Gpio:", gpio)
		line = gpiod.find_line(gpio)

	if line == None:
		print("Not Found")
		exit(1)
	print(line.offset())
	#lines = chip.get_lines([line.offset()])
	lines = chip.get_lines([line.offset()])
	lines.request(consumer='foobar', type=gpiod.LINE_REQ_DIR_OUT, default_vals=[0])
	listlines.append(lines)

while True:
	for lines in listlines:
		listvalues=lines.get_values()
		listnew=[]
		for i in listvalues:
			listnew.append((i+1)%2)
		print(listvalues, listnew)
		lines.set_values(listnew)
		time.sleep(1)

