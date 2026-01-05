#!/usr/bin/env python
import os.path
import glob
import json
import time

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

def find_pwmchip_with_channels(min_channels=2):
    """
    Détecte dynamiquement un pwmchip avec au moins min_channels canaux disponibles.
    Retourne (chip_number, npwm) ou (None, 0) si aucun trouvé.
    """
    for chip_path in sorted(glob.glob("/sys/class/pwm/pwmchip*")):
        try:
            chip_num = int(chip_path.replace("/sys/class/pwm/pwmchip", ""))
            npwm_path = os.path.join(chip_path, "npwm")
            if os.path.exists(npwm_path):
                with open(npwm_path, 'r') as f:
                    npwm = int(f.read().strip())
                if npwm >= min_channels:
                    return chip_num, npwm
        except (ValueError, IOError):
            continue
    return None, 0

# /sys/ pwm interface described here: http://www.jumpnowtek.com/rpi/Using-the-Raspberry-Pi-Hardware-PWM-timers.html
class SysPWM(object):
    chippath = "/sys/class/pwm/pwmchip"

    def __init__(self,chip,pwm):
        self.retry=5
        self.pwm=pwm
        
        # Auto-détection du pwmchip si chip est None
        if chip is None:
            detected_chip, npwm = find_pwmchip_with_channels(min_channels=2)
            if detected_chip is None:
                raise SysPWMException("No PWM chip found with sufficient channels (>=2). Check dtoverlay configuration in /boot/firmware/config.txt and reboot.")
            chip = detected_chip
            print(f"[SysPWM] Auto-detected pwmchip{chip} with {npwm} channels")
        
        # Stocker le numéro du chip pour accès ultérieur
        self.chip = chip
        self.chippath="{chippath}{num}".format(chippath=self.chippath, num=chip)
        self.pwmdir="{chippath}/pwm{pwm}".format(chippath=self.chippath, pwm=self.pwm)
        
        # #region agent log
        try:
            with open("/Users/apple/Documents/Dev - Projets - hors Herd/AstraDIY/.cursor/debug.log", "a") as f:
                f.write(json.dumps({"sessionId":"debug-session","runId":"init","hypothesisId":"A,B,C,D","location":"syspwm.py:__init__","message":"SysPWM init","data":{"chip":chip,"pwm":pwm,"chippath":self.chippath,"pwmdir":self.pwmdir,"chip_available":self.pwmchip_available(),"export_writable":self.export_writable(),"pwmX_exists":self.pwmX_exists()},"timestamp":int(time.time()*1000)})+"\n")
        except: pass
        # #endregion
        
        if not self.pwmchip_available():
            print("On="+self.chippath)
            raise SysPWMException("PWM chip {chip} not available. Check dtoverlay configuration in /boot/firmware/config.txt and reboot.".format(chip=chip))
        if not self.export_writable():
            raise SysPWMException("Need write access to files in '{chippath}'".format(chippath=self.chippath))
        if not self.pwmX_exists():
            self.create_pwmX()
        
        # #region agent log
        try:
            with open("/Users/apple/Documents/Dev - Projets - hors Herd/AstraDIY/.cursor/debug.log", "a") as f:
                f.write(json.dumps({"sessionId":"debug-session","runId":"init","hypothesisId":"C","location":"syspwm.py:__init__","message":"After create_pwmX","data":{"pwmX_exists":self.pwmX_exists(),"pwmdir":self.pwmdir},"timestamp":int(time.time()*1000)})+"\n")
        except: pass
        # #endregion
        return

    def pwmchip_available(self):
        """Vérifie si le pwmchip est disponible (anciennement overlay_loaded)."""
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
        # #region agent log
        try:
            with open("/Users/apple/Documents/Dev - Projets - hors Herd/AstraDIY/.cursor/debug.log", "a") as f:
                f.write(json.dumps({"sessionId":"debug-session","runId":"init","hypothesisId":"C","location":"syspwm.py:create_pwmX","message":"Exporting PWM channel","data":{"pwm":self.pwm,"export_path":pwmexport},"timestamp":int(time.time()*1000)})+"\n")
        except: pass
        # #endregion
        result = self.echo(self.pwm,pwmexport)
        # #region agent log
        try:
            with open("/Users/apple/Documents/Dev - Projets - hors Herd/AstraDIY/.cursor/debug.log", "a") as f:
                f.write(json.dumps({"sessionId":"debug-session","runId":"init","hypothesisId":"C","location":"syspwm.py:create_pwmX","message":"After export","data":{"export_success":result,"pwmX_exists":self.pwmX_exists()},"timestamp":int(time.time()*1000)})+"\n")
        except: pass
        # #endregion

    def enable(self,disable=False):
        enable = "{pwmdir}/enable".format(pwmdir=self.pwmdir)
        num = 1
        if disable:
            num = 0
        # #region agent log
        try:
            with open("/Users/apple/Documents/Dev - Projets - hors Herd/AstraDIY/.cursor/debug.log", "a") as f:
                f.write(json.dumps({"sessionId":"debug-session","runId":"init","hypothesisId":"C","location":"syspwm.py:enable","message":"Enabling PWM","data":{"pwm":self.pwm,"chip":self.chip,"enable":num,"enable_path":enable},"timestamp":int(time.time()*1000)})+"\n")
        except: pass
        # #endregion
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
        # #region agent log
        try:
            with open("/Users/apple/Documents/Dev - Projets - hors Herd/AstraDIY/.cursor/debug.log", "a") as f:
                f.write(json.dumps({"sessionId":"debug-session","runId":"runtime","hypothesisId":"E","location":"syspwm.py:set_duty_ms","message":"Setting duty cycle","data":{"pwm":self.pwm,"chip":self.chip,"duty_ms":milliseconds,"duty_us":microsec},"timestamp":int(time.time()*1000)})+"\n")
        except: pass
        # #endregion
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
    # Utiliser auto-détection pour compatibilité kernel 6.12+
    pwm = SysPWM(None, 1)
    #if pwm.get_periode_ms() != 0:
    pwm.set_duty_ms(0)
    pwm.set_periode_ms(periode1)
    pwm.set_duty_ms(duty1)
    atexit.register(pwm.disable)
    pwm.enable()

    pwm1 = SysPWM(None, 2)
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
