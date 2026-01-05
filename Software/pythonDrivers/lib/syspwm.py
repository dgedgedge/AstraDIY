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
import os

class SysPWMException(Exception):
    pass

def _get_debug_log_path():
    """Retourne le chemin du fichier de log de debug"""
    # Utiliser /tmp pour éviter les problèmes de permissions
    return "/tmp/astradiy_debug.log"

def _write_debug_log(session_id, run_id, hypothesis_id, location, message, data):
    """Écrit un log de debug"""
    try:
        log_path = _get_debug_log_path()
        os.makedirs(os.path.dirname(log_path), exist_ok=True)
        with open(log_path, "a") as f:
            f.write(json.dumps({"sessionId":session_id,"runId":run_id,"hypothesisId":hypothesis_id,"location":location,"message":message,"data":data,"timestamp":int(time.time()*1000)})+"\n")
    except Exception as e:
        print(f"[DEBUG LOG ERROR] {e}")

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
        _write_debug_log("debug-session", "init", "A,B,C,D", "syspwm.py:__init__", "SysPWM init", {"chip":chip,"pwm":pwm,"chippath":self.chippath,"pwmdir":self.pwmdir,"chip_available":self.pwmchip_available(),"export_writable":self.export_writable(),"pwmX_exists":self.pwmX_exists()})
        # #endregion
        
        if not self.pwmchip_available():
            print("On="+self.chippath)
            raise SysPWMException("PWM chip {chip} not available. Check dtoverlay configuration in /boot/firmware/config.txt and reboot.".format(chip=chip))
        if not self.export_writable():
            raise SysPWMException("Need write access to files in '{chippath}'".format(chippath=self.chippath))
        if not self.pwmX_exists():
            self.create_pwmX()
        
        # #region agent log
        _write_debug_log("debug-session", "init", "C", "syspwm.py:__init__", "After create_pwmX", {"pwmX_exists":self.pwmX_exists(),"pwmdir":self.pwmdir})
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
        _write_debug_log("debug-session", "init", "C", "syspwm.py:create_pwmX", "Exporting PWM channel", {"pwm":self.pwm,"export_path":pwmexport,"chip":self.chip})
        # #endregion
        result = self.echo(self.pwm,pwmexport)
        # Lire le GPIO réellement utilisé après export
        gpio_info = {}
        gpio_number = None
        try:
            if os.path.exists(self.pwmdir):
                # Lire uevent pour obtenir les informations du périphérique
                uevent_path = os.path.join(self.pwmdir, "uevent")
                if os.path.exists(uevent_path):
                    with open(uevent_path, 'r') as f:
                        uevent_content = f.read()
                        gpio_info["uevent"] = uevent_content
                        # Extraire GPIO depuis uevent si présent
                        for line in uevent_content.split('\n'):
                            if 'GPIO' in line.upper() or 'PIN' in line.upper():
                                gpio_info["uevent_gpio_line"] = line
                
                # Chercher le lien symbolique pour obtenir le chemin du périphérique
                if os.path.islink(self.pwmdir):
                    real_path = os.readlink(self.pwmdir)
                    gpio_info["real_path"] = real_path
                    # Extraire le numéro de GPIO si possible depuis le chemin
                    if "gpio" in real_path.lower():
                        parts = real_path.split("/")
                        for part in parts:
                            if "gpio" in part.lower():
                                gpio_info["gpio_from_path"] = part
                                # Essayer d'extraire le numéro
                                import re
                                match = re.search(r'(\d+)', part)
                                if match:
                                    gpio_number = int(match.group(1))
                                break
                
                # Lire le fichier device/of_node pour obtenir le GPIO depuis le device tree
                device_path = os.path.join(self.pwmdir, "device")
                if os.path.exists(device_path):
                    if os.path.islink(device_path):
                        device_real = os.readlink(device_path)
                        gpio_info["device_path"] = device_real
                        # Chercher dans le device tree
                        of_node_path = os.path.join(self.pwmdir, "device", "of_node")
                        if os.path.exists(of_node_path):
                            gpio_info["of_node_exists"] = True
                            # Lire les propriétés du device tree
                            try:
                                compatible_path = os.path.join(of_node_path, "compatible")
                                if os.path.exists(compatible_path):
                                    with open(compatible_path, 'r') as f:
                                        gpio_info["compatible"] = f.read().strip()
                            except:
                                pass
        except Exception as e:
            gpio_info["error"] = str(e)
        
        # Afficher dans la console pour diagnostic immédiat
        print(f"[DEBUG GPIO MAPPING] pwmchip{self.chip} canal {self.pwm}: GPIO={gpio_number}, path={self.pwmdir}, info={gpio_info.get('gpio_from_path', 'N/A')}")
        # #region agent log
        _write_debug_log("debug-session", "init", "A,D", "syspwm.py:create_pwmX", "After export - GPIO mapping", {"export_success":result,"pwmX_exists":self.pwmX_exists(),"pwmdir":self.pwmdir,"chip":self.chip,"pwm":self.pwm,"gpio_info":gpio_info})
        # #endregion

    def enable(self,disable=False):
        enable = "{pwmdir}/enable".format(pwmdir=self.pwmdir)
        num = 1
        if disable:
            num = 0
        # #region agent log
        _write_debug_log("debug-session", "init", "C", "syspwm.py:enable", "Enabling PWM", {"pwm":self.pwm,"chip":self.chip,"enable":num,"enable_path":enable})
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
        _write_debug_log("debug-session", "runtime", "E", "syspwm.py:set_duty_ms", "Setting duty cycle", {"pwm":self.pwm,"chip":self.chip,"duty_ms":milliseconds,"duty_us":microsec})
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
