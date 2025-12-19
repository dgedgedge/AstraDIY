#!/usr/bin/env python
import os.path

# Copyright 2018 Jeremy Impson <jdimpson@acm.org>

# This program is free software; you can redistribute it and/or modify it 
# under the terms of the GNU General Public License as published by the Free 
# Software Foundation; either version 3 of the License, or (at your option) 
# any later version.
#
# This program is distributed in the hope that it will be useful, but 
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
# or FITNESS FOR A PARTICULAR PURPOSE. 
# See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, see <http://www.gnu.org/licenses>.

import time

class SysPWMException(Exception):
    pass

# /sys/ pwm interface described here: http://www.jumpnowtek.com/rpi/Using-the-Raspberry-Pi-Hardware-PWM-timers.html
class SysPWM(object):
    chippath = "/sys/class/pwm/pwmchip"

    def __init__(self,chip,pwm):
        self.retry=5
        self.pwm=pwm
        self.chippath="{chippath}{num}".format(chippath=self.chippath, num=chip)
        self.pwmdir="{chippath}/pwm{pwm}".format(chippath=self.chippath, pwm=self.pwm)
        if not self.overlay_loaded():
            print("On="+self.chippath)
            raise SysPWMException("Need to add 'dtoverlay=pwm-2chan' to /boot/firmware/config.txt and reboot")
        if not self.export_writable():
            raise SysPWMException("Need write access to files in '{chippath}'".format(chippath=self.chippath))
        if not self.pwmX_exists():
            self.create_pwmX()
        return

    def overlay_loaded(self):
        return os.path.isdir(self.chippath)

    def export_writable(self):
        return os.access("{chippath}/export".format(chippath=self.chippath), os.W_OK)

    def pwmX_exists(self):
        return os.path.isdir(self.pwmdir)

    def echo(self,m,fil):
        gotValue=False
        retry=self.retry
        while(not(gotValue) and retry >0):
            try:
                #print "echo {m} > {fil}".format(m=m,fil=fil)
                with open(fil,'w') as f:
                    f.write("{m}\n".format(m=m))
                gotValue=True
            except Exception as e:
                time.sleep(1)
            retry=retry-1
        if not(gotValue):
            try:
                #print "echo {m} > {fil}".format(m=m,fil=fil)
                with open(fil,'w') as f:
                    f.write("{m}\n".format(m=m))
                gotValue=True
            except Exception as e:
                print("Uable to open ", fil, " Exception ",str(e), "Arg=",m)
        return gotValue

    def create_pwmX(self):
        pwmexport = "{chippath}/export".format(chippath=self.chippath)
        self.echo(self.pwm,pwmexport)

    def enable(self,disable=False):
        enable = "{pwmdir}/enable".format(pwmdir=self.pwmdir)
        num = 1
        if disable:
            num = 0
        self.echo(num,enable)

    def disable(self):
        return self.enable(disable=True)

    def set_duty_us(self,microsec):
        # /sys/ iface, 2ms is 2000000
        # gpio cmd,    2ms is 200
        dc = int(microsec * 1000)
        duty_cycle = "{pwmdir}/duty_cycle".format(pwmdir=self.pwmdir)
        #print(duty_cycle,self.chippath)
        self.echo(dc,duty_cycle)

    def set_duty_ms(self,milliseconds):
        # /sys/ iface, 2ms is 2000000
        # gpio cmd,    2ms is 200
        microsec = int(milliseconds * 1000)
        self.set_duty_us(microsec)

    def get_periode_ms(self):
        retval=0
        fil="{pwmdir}/period".format(pwmdir=self.pwmdir)
        with open(fil,'r') as f:
            retval=f.read()
        retval = int(retval)/1000000
        #print("get_periode_ms=", str(retval))
        return int(retval)    

    def set_periode_us(self,per):
        per *= 1000 # now in.. whatever
        per = int(per)
        period = "{pwmdir}/period".format(pwmdir=self.pwmdir)
        #print("periode:",per,", File:", period)
        self.echo(per,period)

    def set_periode_ms(self,per):
        per *= 1000 # now in.. whatever
        self.set_periode_us(per)

    def set_frequency(self,hz):
        per = (1 / float(hz))
        per *= 1000    # now in milliseconds
        self.set_periode_ms(per)

listpwm=[]
def myatexit():
    for pwm in listpwm:
        pwm.disable()    

if __name__ == "__main__":
    from time import sleep
    import atexit
    SLEE=0.5
    periode1=1
    periode2=1
    step=0.001
    duty1=0
    duty2=periode2

    #pwm0 is GPIO pin 18 is physical pin 12
    
    # OK 18, 13
    pwm = SysPWM(2,1)
    #if pwm.get_periode_ms() != 0:
    pwm.set_duty_ms(0)
    pwm.set_periode_ms(periode1)
    pwm.set_duty_ms(duty1)
    atexit.register(pwm.disable)
    pwm.enable()

    pwm1 = SysPWM(2,2)
    #if pwm1.get_periode_ms() != 0:
    pwm1.set_duty_ms(0)
    pwm1.set_periode_ms(periode2)
    pwm1.set_duty_ms(duty2)
    atexit.register(pwm1.disable)
    pwm1.enable()

    while True:
        duty1 = (duty1 + step)
        if duty1 > periode1:
            duty1=0
        print("Duty1:",duty1, "ms", "period=",periode1,"ms")
        pwm.set_duty_ms(duty1)
        duty2 = (duty2 - step)
        if duty2 < 0:
            duty2=periode2
        print("Duty2:",duty2, "ms", "period=",periode2,"ms")
        pwm1.set_duty_ms(duty2)
        sleep(SLEE)
    sleep(1000000)
