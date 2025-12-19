#!/usr/bin/env python3
import sys
import os
import signal
from PyQt5.QtCore import QTimer
from PyQt5.QtWidgets import QApplication, QMainWindow, QWidget, QPushButton, QVBoxLayout
from PyQt5.QtWidgets import QHBoxLayout, QLineEdit, QLabel, QFrame, QComboBox
from PyQt5.QtCore import Qt
from AstraGpio import AstraGpio
from AstraPwm import AstraPwm


class dataMenu(QWidget):
    def __init__(self, label, unit):
        self.unit=unit
        self.label=label
        super().__init__()
        self.initUI()

    def initUI(self):
        self.type_label = QLabel(self.label, self)  
        self.type_label.setAlignment(Qt.AlignCenter)
        #self.type_label.setFixedHeight(50)

        self.line_edit = QLineEdit(self)
        #self.line_edit.setInputMask('9999')  # Limite les caractères à des chiffres uniquement
        #self.line_edit.setFixedHeight(50)

        self.unit_label = QLabel(self.unit, self)  
        self.unit_label.setAlignment(Qt.AlignCenter)
        #self.unit_label.setFixedHeight(50)

        # Mettre le QLabel et le QLineEdit dans un QHBoxLayout pour les aligner horizontalement
        layout = QHBoxLayout()
        layout.setSpacing(0)
        layout.addWidget(self.type_label)
        layout.addWidget(self.line_edit)
        layout.addWidget(self.unit_label)
        self.setLayout(layout)

    def setText(self, value):
        self.line_edit.setText(value)

    def setInputMask(self, value):
        self.line_edit.setInputMask(value)

    def getText(self):
        return self.line_edit.text()

    def setFixedWidth(self, w1, w2, w3):
        self.type_label.setFixedWidth(w1)
        self.line_edit.setFixedWidth(w2)
        self.unit_label.setFixedWidth(w3)

    def setReadOnly(self, mybool):
        self.line_edit.setReadOnly(mybool)

    def connect(self, doSomething):
        self.line_edit.textEdited.connect(doSomething)


class ina219Frame(QFrame):
    def __init__(self, ina219):
        super().__init__()
        self.ina219 = ina219
        self.initUI()
        
    def initUI(self):
        self.setStyleSheet("border: 1px solid black;") 
        
        # Voltage
        self.textVoltage = dataMenu("Tension", "V")
        self.textVoltage.setFixedWidth(80,70,50)
        self.textVoltage.setReadOnly(True)

        # Current 
        self.textCurrent = dataMenu("Courant", "mA")
        self.textCurrent.setFixedWidth(80,70,50)
        self.textCurrent.setReadOnly(True)

        # PowerQT
        self.textEnergie = dataMenu("Energie", "Ah")
        self.textEnergie.setFixedWidth(80,70,50)
        self.textEnergie.setReadOnly(True)

        layout = QVBoxLayout()
        layout.setSpacing(0)
        layout.addWidget(self.textVoltage)
        layout.addWidget(self.textCurrent)
        layout.addWidget(self.textEnergie)
        self.setLayout(layout)
        self.setFixedSize(230, 140)

    def update_text_fields(self):
        bus_voltage = self.ina219.voltage()
        current = self.ina219.current()
        energie = self.ina219.energie()/3600/1000

        self.textVoltage.setText(f"{bus_voltage:+.1f}")
        self.textCurrent.setText(f"{current:+.1f}")
        if energie < 100:
            self.textEnergie.setText(f"{energie:.3f}")
        elif energie < 500:
            self.textEnergie.setText(f"{energie:.2f}")
        elif energie < 1000:
            self.textEnergie.setText(f"{energie:.1f}")
        else:
            energie=int(energie/1000)
            self.textEnergie.setText(f"{energie}k")

class GpioControl(QWidget):
    def __init__(self, name):
        super().__init__()
        self.name=name
        self.gpio = AstraGpio(name)
        self.initUI()

    def initUI(self):
        #self.setStyleSheet("border: 1px solid black;") 
        
        # Zone de texte initiale
        self.TextName = QLabel(self.name, self)
        #self.TextName.setReadOnly(True)
        self.TextName.setFixedWidth(100)

        # Bouton On/Off
        self.toggle_button = QPushButton('Off set On', self)
        self.toggle_button.setFixedWidth(100)
        self.toggle_button.setStyleSheet("border: 1px solid black;") 
        self.set_togglebuttonText()
        self.toggle_button.setCheckable(True)
        self.toggle_button.clicked.connect(self.toggle_action)

        # InaFrame
        self.inaFrame = ina219Frame(self.gpio.get_ina219())
      
        # Layout
        layout = QHBoxLayout()
        layout.setSpacing(0)
        layout.addWidget(self.TextName)
        layout.addWidget(self.toggle_button)
        layout.addWidget(self.inaFrame)
        self.setLayout(layout)

    def set_togglebuttonText(self):
        if self.gpio.is_on():
            self.toggle_button.setText('On Set Off')
            self.toggle_button.setStyleSheet("background-color: #f75457; border: 1px solid black;")
        else:
            self.toggle_button.setText('Off Set On')
            self.toggle_button.setStyleSheet("background-color: #3cbaa2; border: 1px solid black;") 


    def toggle_action(self):
        self.gpio.switch_onoff()
        self.set_togglebuttonText()
        self.gpio.print_status()

    def update_text_fields(self):
        self.inaFrame.update_text_fields()


class DrewControl(QWidget):
    def __init__(self, name):
        super().__init__()
        self.name=name
        self.buttonOn=True
        self.AstraDrew = AstraPwm(name)
        self.initUI()

    def initUI(self):
  
        # Button 
        self.toggle_button = QPushButton(self.name+' Off', self)
        self.toggle_button.setCheckable(True)
        self.toggle_button.clicked.connect(self.toggle_action)

        self.save_button = QPushButton('Save', self)
        self.save_button.setCheckable(True)
        self.save_button.clicked.connect(self.AstraDrew.save)


        # Selection capteur temperature
        self.selTemp = QComboBox(self)        
        curtempname = self.AstraDrew. get_associateTemp()
        defaultIndex=0
        curindex=0
        for tempId in self.AstraDrew.get_listTemp():
            self.selTemp.addItem(tempId)
            if tempId == curtempname:
                defaultIndex=curindex
            curindex=curindex+1
        self.selTemp.setCurrentIndex(defaultIndex)
        #self.set_associateTemp(0)
        self.selTemp.currentIndexChanged.connect(self.set_associateTemp)

        # Power
        self.textPower = dataMenu("Power", "%")
        self.textPower.setFixedWidth(100,70,15)
        self.textPower.setReadOnly(False)
        #self.textPower.setInputMask("000")
        self.textPower.connect(self.set_power)

        # Temp consigne
        self.textTempConsigne = dataMenu("Consigne", "°C")
        self.textTempConsigne.setFixedWidth(100,70,15)
        self.textTempConsigne.setReadOnly(False)
        self.textTempConsigne.connect(self.set_cmdtemp)

        # Measure Temp
        self.textTempMesure = dataMenu("Measure", "°C")
        self.textTempMesure.setFixedWidth(100,70,15)
        self.textTempMesure.setReadOnly(True)


         # InaFrame
        self.inaFrame = ina219Frame(self.AstraDrew.get_ina219())

        # Set defauls
        self.set_buttonOff()
        self.set_togglebuttonText()
        self.textTempMesure.setText("10")
        self.textTempConsigne.setText("10")

        firstCol = QVBoxLayout()
        firstCol.setSpacing(0)
        firstCol.addWidget(self.toggle_button)        
        firstCol.addWidget(self.selTemp)        
        firstCol.addWidget(self.save_button)        

        # Layout
        secCol = QVBoxLayout()
        secCol.setSpacing(0)
        secCol.addWidget(self.textPower)        
        secCol.addWidget(self.textTempConsigne)
        secCol.addWidget(self.textTempMesure)        

        thirdCol = QVBoxLayout()
        thirdCol.setSpacing(0)
        thirdCol.addWidget(self.inaFrame)        

        allLayout = QHBoxLayout()
        allLayout.setSpacing(0)
        allLayout.addLayout(firstCol)
        allLayout.addLayout(secCol)
        allLayout.addLayout(thirdCol)
        self.setLayout(allLayout)

    def set_textPowerReadOnly(self, val):
        self.textPower.setReadOnly(val)

    def set_togglebuttonText(self):
        if self.buttonOn:
            self.toggle_button.setText("auto "+self.name+' is On Set Off')
            self.toggle_button.setStyleSheet("background-color: #f75457; border: 1px solid black;")
            self.AstraDrew.startAserv()
        else:
            self.toggle_button.setText("auto "+self.name+' is Off Set On')
            self.toggle_button.setStyleSheet("background-color: #3cbaa2; border: 1px solid black;")
            if self.AstraDrew.isAserv():
                self.AstraDrew.stopAserv()
                self.AstraDrew.set_ratio(0)
            self.textPower.setText(str(self.AstraDrew.get_ratio()))

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
            self.set_buttonOff()

    def set_cmdtemp(self):
        self.AstraDrew.set_cmdTemp(self.textTempConsigne.getText())

    def toggle_action(self):
        self.buttonOn = not(self.buttonOn)
        self.set_togglebuttonText()

    def set_buttonOff(self):
        self.buttonOn = False
        self.set_togglebuttonText()

    def update_text_fields(self):
        if self.buttonOn:
            ratio=self.AstraDrew.get_ratio()
            self.textPower.setText(f"{ratio:.1f}")
            self.textPower.setReadOnly(True)
        else:
            self.textPower.setReadOnly(False)
        self.inaFrame.update_text_fields()
        temp=self.AstraDrew.get_temp()
        self.textTempMesure.setText(f"{temp:+.1f}")

class MainWindow(QWidget):
    def __init__(self):
        super().__init__()
        self.initUI()


    def initUI(self):
        self.main_layout = QVBoxLayout()
        self.main_layout.setSpacing(0)

        self.widgets = []
        for name in ["AstraDc1", "AstraDc2", "AstraDc3"]:
            wiget=GpioControl(name)
            self.widgets.append(wiget)
            self.main_layout.addWidget(wiget)
        
        for name in ["AstraPwm1", "AstraPwm2"]:
            wiget=DrewControl(name)
            self.widgets.append(wiget)
            self.main_layout.addWidget(wiget)


        self.setLayout(self.main_layout)
        self.setWindowTitle('AstrAlim')


        # Créer un timer pour mettre à jour tous les widgets toutes les secondes
        self.timer = QTimer()
        self.timer.timeout.connect(lambda: [widget.update_text_fields() for widget in self.widgets])
        self.timer.start(1000)  # Met à jour toutes les 1000 millisecondes (1 seconde)
 
    def closeEvent(self, event):
        os.kill(os.getpid(), signal.SIGTERM)

if __name__ == '__main__':
    app = QApplication(sys.argv)
    main_window = MainWindow()
    main_window.show()


    sys.exit(app.exec_())

