#!/usr/bin/env python3
# GPIO used PA17

import gpiod
import time


class AstraGpio():
    astraGpioSet = { 
                "AstraDc1": {"pin": 37},
                "AstraDc2": {"pin": 38},
                "AstraDc3": {"pin": 40},
    }
    pin2gpio = { 37:26, 38:20, 40:21}
    def __init__(self, name):
        self.name = name
        if name in self.astraGpioSet :
            self.inacaract=self.astraGpioSet[self.name]
        else:
            raise Exception("Unkown AstraGpi")
         
        pin=self.inacaract["pin"]
        self.gpioline = gpiod.find_line(f"PIN{pin:d}")
        if self.gpioline == None: 
            self.gpioline = gpiod.find_line(f"PIN{pin:d}")
        if self.gpioline == None: 
            self.gpioline = gpiod.find_line(f"GPIO{self.pin2gpio[pin]:d}")
        if self.gpioline == None: 
            self.gpioline = gpiod.find_line(f"GPIO{self.pin2gpio[pin]:d}")
        if self.gpioline == None:
            raise Exception("Gpio Not Found")
        #self.gpioline.request(consumer='AstrAlim', type=gpiod.LINE_REQ_DIR_OUT, default_vals=[0])
        try:
            self.gpioline.request(consumer='AstrAlim')
        except OSError as e:
            if e.errno == 16:  # Device or resource busy
                # Vérifier qui utilise la GPIO
                consumer = self.gpioline.consumer()
                if consumer:
                    raise Exception(f"GPIO {self.gpioline.name()} est déjà utilisée par '{consumer}'. "
                                  f"Fermez l'autre instance du programme ou redémarrez le système.")
                else:
                    raise Exception(f"GPIO {self.gpioline.name()} est déjà utilisée. "
                                  f"Fermez l'autre instance du programme ou redémarrez le système.")
            else:
                raise
        if self.gpioline.direction() != self.gpioline.DIRECTION_OUTPUT:
            print("Set dirout at init")
            self.gpioline.set_direction_output()

    def print_status(self):
        print(self.name,":",self.gpioline.name(), ":", self.gpioline.get_value())

    def get_name(self):
        return self.gpioline.name()

    def set_on(self):
        self.gpioline.set_value(1)

    def set_off(self):
        self.gpioline.set_value(0)

    def switch_onoff(self):
        if self.gpioline.get_value() == 0:
            self.gpioline.set_value(1)
        else:
            self.gpioline.set_value(0)

    def is_on(self):
        return (self.gpioline.get_value() != 0)


if __name__ == "__main__":
    import argparse
    import os

    parser = argparse.ArgumentParser( prog=os.path.basename(__file__), description='Program that enable to set AstrAlim Gpio.', epilog='', usage='%(prog)s [options]')
    parser.add_argument('--verbose', '-v', action='count', default=0)
    parser.add_argument('--test', action='store_true', help='If specified and no othe params, then a test set will be run.')
    parser.add_argument('--gpioOn', nargs='+', choices=AstraGpio.astraGpioSet.keys(), default=[], help='Enable to set On the AstrAlim Gpio.')
    parser.add_argument('--gpioOff', nargs='+', choices=AstraGpio.astraGpioSet.keys(), default=[], help='Enable to set Off the AstrAlim Gpio.')
    args = parser.parse_args()

    if len(args.gpioOn) > 0:
        for interface in args.gpioOn:
            if args.verbose:
                print("Set On =", interface)
            AstraGpio(interface).set_on()
    elif len(args.gpioOff) > 0:
        for interface in args.gpioOff:
            if args.verbose:
                print("Set Off =", interface)
            AstraGpio(interface).set_off()
    else:
        if args.test:
            listastragpio = []
            for name in  [ "AstraDc1", "AstraDc2", "AstraDc3" ]:
                listastragpio.append(AstraGpio(name))
            while True:
                for line in listastragpio:
                    line.switch_onoff()
                    time.sleep(1)
                    line.print_status()
        else:
            parser.print_help()

