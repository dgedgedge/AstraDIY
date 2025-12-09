# Warning :
Those scripts where developped with stellarmate 1.8.

# Overall:
To install all please run "sudo ./install.sh"

But you may install the parts as specified below :

# GPS config :
   * To install gps please run "sudo install_gps.sh"
   * GPS installation was driven by the following : [Microsecond accurate NTP with a Raspberry Pi and PPS GPS](https://austinsnerdythings.com/2021/04/19/microsecond-accurate-ntp-with-a-raspberry-pi-and-pps-gps/)

# Scripts and boot config :
   * To Install Scripts please run "install_scripts.sh"
   * Scripts are installed in /opt/AstrAlim a reboot is needed for the scripts do be accessible.
   * As the scripts need some rpi configuration to enable i2c, 1-Wire, pwm allocation ... the file /boot/firmware/config.txt shall be adapted. This can be done through the  install_bootConfig.sh script. 


