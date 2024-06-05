#!/bin/env python3
import sys
import os
import signal
from PyQt5.QtCore import QTimer
from PyQt5.QtWidgets import QApplication, QMainWindow, QWidget, QPushButton, QVBoxLayout
from PyQt5.QtWidgets import QHBoxLayout, QLineEdit, QLabel, QFrame, QComboBox
from PyQt5.QtCore import Qt
from AstraPwm import AstraPwm
from AstraCommonHmi import dataMenu, AnimatedToggleButton


class DrewControl(QWidget):
    def __init__(self, name):
        super().__init__()
        self.name=name
        self.buttonAsservOn=False
        self.buttonRoseeConsigneOn=False
        self.AstraDrew = AstraPwm(name)

        #####################################
        # button zone
        # Button Asserv
        self.toggle_buttonAsserv = AnimatedToggleButton(parent=self, toggle_callback=self.set_togglebuttonAsserv, initial_state=self.buttonAsservOn)
        labelButtonAsserv = QLabel("Asserv Temp", self)  
        labelButtonAsserv.setAlignment(Qt.AlignCenter)
        labelButtonAsserv.setFixedHeight(50)
        labelButtonAsserv.adjustSize()

        # Asserv temp to rosee
        self.toggle_buttonRoseeConsigne = AnimatedToggleButton(parent=self, toggle_callback=self.set_togglebuttonRoseeConsigne, initial_state=self.buttonRoseeConsigneOn)
        labelButtonRosee = QLabel("Asserv Consigne", self)  
        labelButtonRosee.setAlignment(Qt.AlignCenter)
        labelButtonRosee.setFixedHeight(50)
        labelButtonRosee.adjustSize()
        
        self.save_button = QPushButton('Save', self)
        self.save_button.setCheckable(True)
        self.save_button.clicked.connect(self.AstraDrew.save)


        # Selection capteur temperature
        self.selTemp = QComboBox(self)        
        curtempname = self.AstraDrew.get_associateTemp()
        defaultIndex=0
        self.selTemp.addItem("Unset Temp Sensor")
        curindex=1
        if len(self.AstraDrew.get_listTemp()) > 0:
            for tempId in self.AstraDrew.get_listTemp():
                self.selTemp.addItem(tempId)
                if tempId == curtempname:
                    defaultIndex=curindex
                curindex=curindex+1
        else:
            defaultIndex=0
        self.selTemp.setCurrentIndex(defaultIndex)
        #self.set_associateTemp(0)
        self.selTemp.currentIndexChanged.connect(self.set_associateTemp)
        
        # Lay Out
        firstCol = QVBoxLayout()
        firstCol.setSpacing(0)
        firstCol.addWidget(labelButtonAsserv)        
        firstCol.addWidget(self.toggle_buttonAsserv)        
        firstCol.addWidget(labelButtonRosee)        
        firstCol.addWidget(self.toggle_buttonRoseeConsigne)        
        firstCol.addWidget(self.selTemp)        
        firstCol.addWidget(self.save_button)        

        #####################################
        # Control zone
        # Power
        self.textPower = dataMenu("Power", "%")
        self.textPower.setFixedWidth(100,70,15)
        self.textPower.setReadOnly(False)
        #self.textPower.setInputMask("000")
        self.textPower.connect(self.set_power)

        # Temp consigne
        self.textTempConsigne = dataMenu("Consigne", "°C")
        self.textTempConsigne.setFixedWidth(100,70,20)
        self.textTempConsigne.setReadOnly(False)
        self.textTempConsigne.connect(self.set_cmdtemp)

        # Measure Temp
        self.textTempMesure = dataMenu("Measure", "°C")
        self.textTempMesure.setFixedWidth(100,70,20)
        self.textTempMesure.setReadOnly(True)
        
        # layout
        secCol = QVBoxLayout()
        secCol.setSpacing(0)
        secCol.addWidget(self.textPower)        
        secCol.addWidget(self.textTempConsigne)
        secCol.addWidget(self.textTempMesure)        

        #####################################
        # Bme zone
        # Control zone
        # bme temp
        self.textBmeTemp = dataMenu("envTemp", "°C")
        self.textBmeTemp.setFixedWidth(100,70,20)
        self.textBmeTemp.setReadOnly(True)
        #self.textBmeTemp.setInputMask("000")

        # Bme Hmidity
        self.textHumidity = dataMenu("Humidity", "%")
        self.textHumidity.setFixedWidth(100,70,15)
        self.textHumidity.setReadOnly(True)

        # Temp Rosee
        self.textRosee = dataMenu("TempRosee", "°C")
        self.textRosee.setFixedWidth(100,70,20)
        self.textRosee.setReadOnly(True)

        # Set defauls
        self.set_togglebuttonRoseeConsigne(False)
        self.set_togglebuttonAsserv(False)
        self.textTempMesure.setText("10")
        self.textTempConsigne.setText("10")

        #####################################
        # Pid zone
        self.textKp = dataMenu("Kp", "[0-100]")
        self.textKp.setFixedWidth(100,70,120)
        self.textKp.setReadOnly(False)

        self.textKi = dataMenu("Ki", "[0-100]")
        self.textKi.setFixedWidth(100,70,120)
        self.textKi.setReadOnly(False)

        self.textKd = dataMenu("Kd", "[0-100]")
        self.textKd.setFixedWidth(100,70,120)
        self.textKd.setReadOnly(False)

        # layout
        thirdCol = QVBoxLayout()
        thirdCol.setSpacing(0)
        thirdCol.addWidget(self.textBmeTemp)        
        thirdCol.addWidget(self.textHumidity)
        thirdCol.addWidget(self.textRosee)        

        # layout
        forthCol = QVBoxLayout()
        forthCol.setSpacing(0)
        forthCol.addWidget(self.textKp)        
        forthCol.addWidget(self.textKi)
        forthCol.addWidget(self.textKd)        

        # Layout

        allLayout = QHBoxLayout()
        allLayout.setSpacing(0)
        allLayout.addLayout(firstCol)
        allLayout.addLayout(secCol)
        allLayout.addLayout(thirdCol)
        allLayout.addLayout(forthCol)

        title = QLabel(self.name, self)  
        title.setAlignment(Qt.AlignCenter)
        title.adjustSize()

        allLayoutTite = QVBoxLayout()
        allLayoutTite.addWidget(title)
        allLayoutTite.addLayout(allLayout)
        self.setLayout(allLayoutTite)
        


    def set_textPowerReadOnly(self, val):
        self.textPower.setReadOnly(val)

    def set_associateTemp(self, index):
        selected_item_text = self.selTemp.itemText(index)
        self.AstraDrew.set_associateTemp(selected_item_text)
        #print("Selected Temp Sensor:", selected_item_text)

    def set_power(self):
        ratio=self.textPower.getText()
        try:
            ratio=int(ratio)
        except:
            pass
        else:
            #print("self.AstraDrew.set_ratio(",ratio,")")
            self.AstraDrew.set_ratio(ratio)

    def set_cmdtemp(self):
        self.AstraDrew.set_cmdTemp(self.textTempConsigne.getText())

    def set_togglebuttonRoseeConsigne(self, state):
        self.buttonRoseeConsigneOn = state
        if state:
            self.AstraDrew.set_asservTempRosee()
        else:
            self.AstraDrew.unset_asservTempRosee()

    def set_togglebuttonAsserv(self, state):
        self.buttonAsservOn = state
        if state:
            self.AstraDrew.startAserv()
        else:
            if self.AstraDrew.isAserv():
                self.AstraDrew.stopAserv()
                self.AstraDrew.set_ratio(0)
            self.textPower.setText(str(self.AstraDrew.get_ratio()))

    def update_text_fields(self):
        if self.AstraDrew.get_autoUpdateKpKiKd():
            kp=self.AstraDrew.get_Kp()
            ki=self.AstraDrew.get_Ki()
            kd=self.AstraDrew.get_Kd()
            self.textKp.setText(f"{kp:.3f}")
            self.textKi.setText(f"{ki:.3f}")
            self.textKd.setText(f"{kd:.3f}")
            self.textPower.setReadOnly(True)
        else:
            self.textPower.setReadOnly(False)
        if self.buttonAsservOn:
            ratio=self.AstraDrew.get_ratio()
            self.textPower.setText(f"{ratio:.1f}")
            self.textPower.setReadOnly(True)
        else:
            self.textPower.setReadOnly(False)

        if self.buttonRoseeConsigneOn:
            if not self.buttonAsservOn:
                self.AstraDrew.updateCmdTempfromTempRosee()
            cmdTemp=self.AstraDrew.get_cmdTemp()
            self.textTempConsigne.setText(f"{cmdTemp:.1f}")
            self.textTempConsigne.setReadOnly(True)
        else:
            self.textTempConsigne.setReadOnly(False)

 
        tempsensor=self.AstraDrew.get_temp()
        if tempsensor == self.AstraDrew.TEMPUNAVAIL:
            self.textTempMesure.setText(f"NAvail")
            self.textTempMesure.setStyleSheet("background-color: #f75457; border: 1px solid black;")
        else:
            self.textTempMesure.setText(f"{tempsensor:+.1f}")
            self.textTempMesure.setStyleSheet("background-color: #3cbaa2; border: 1px solid black;")

        tempbme=self.AstraDrew.get_bmeTemp()
        if tempbme == self.AstraDrew.TEMPUNAVAIL:
            self.textBmeTemp.setStyleSheet("background-color: #f75457; border: 1px solid black;")
            self.textBmeTemp.setText(f"N/A")
            self.textHumidity.setText(f"N/A")
            self.textRosee.setText(f"N/A")
        else:
            self.textBmeTemp.setText(f"{tempbme:+.1f}")
            self.textBmeTemp.setStyleSheet("background-color: #3cbaa2; border: 1px solid black;")
            hum=self.AstraDrew.get_bmeHumidity()
            self.textHumidity.setText(f"{hum:+.1f}")
            rosee=self.AstraDrew.get_bmeTempRosee()
            self.textRosee.setText(f"{rosee:+.1f}")

class MainPwmWindow(QWidget):
    def __init__(self):
        super().__init__()
        self.initUI()


    def initUI(self):
        self.main_layout = QVBoxLayout()
        self.main_layout.setSpacing(0)

        self.widgets = []
        for name in ["AstraPwm1", "AstraPwm2"]:
            wiget=DrewControl(name)
            self.widgets.append(wiget)
            self.main_layout.addWidget(wiget)


        self.setLayout(self.main_layout)
        self.setWindowTitle('AstrAlimPwm')


        # Créer un timer pour mettre à jour tous les widgets toutes les secondes
        self.timer = QTimer()
        self.timer.timeout.connect(lambda: [widget.update_text_fields() for widget in self.widgets])
        self.timer.start(1000)  # Met à jour toutes les 1000 millisecondes (1 seconde)
 
    def closeEvent(self, event):
        os.kill(os.getpid(), signal.SIGTERM)

if __name__ == '__main__':
    app = QApplication(sys.argv)
    main_window = MainPwmWindow()
    main_window.show()


    sys.exit(app.exec_())

