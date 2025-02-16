#!/bin/env python3
import sys
import os
import signal
import math
from PyQt5.QtCore import QTimer
from PyQt5.QtWidgets import QApplication, QMainWindow, QWidget, QPushButton, QVBoxLayout, QGridLayout
from PyQt5.QtWidgets import QHBoxLayout, QLineEdit, QLabel, QFrame, QComboBox
from PyQt5.QtCore import Qt
from AstraIna import AstraIna
from AstraCommonHmi import dataMenu

def formatEnergie(energie):
    retval=""
    if not isinstance(energie, (int, float)):
        retval=energie
    elif energie < 100:
        retval=f"{energie:.3f}"
    elif energie < 500:
        retval=f"{energie:.2f}"
    elif energie < 1000:
        retval=f"{energie:.1f}"
    else:
        energie=int(energie/1000)
        retval=f"{energie}k"
    return retval


class ina219Frame(QFrame):
    def __init__(self, ina219:AstraIna, parent=None):
        super().__init__(parent)
        self.ina219:AstraIna = ina219
        #self.setStyleSheet("border: 1px solid black;") 
        
        label = QLabel(self.ina219.getName(), parent=self) 
        label.setAlignment(Qt.AlignCenter)        
        label.adjustSize()
        # Voltage
        self.textVoltageV:dataMenu = dataMenu("Tension", "V", parent=self)
        self.textVoltageV.setFixedWidth(80,70,50)
        self.textVoltageV.setReadOnly(True)

        # Current 
        self.textCurrentA:dataMenu = dataMenu("Courant", "A", parent=self)
        self.textCurrentA.setFixedWidth(80,70,50)
        self.textCurrentA.setReadOnly(True)

        # PowerQT
        self.textPowermW:dataMenu = dataMenu("Power", "mW", parent=self)
        self.textPowermW.setFixedWidth(80,70,50)
        self.textPowermW.setReadOnly(True)

        # EnergieQT
        self.textEnergieWH:dataMenu = dataMenu("Energie", "Wh", parent=self)
        self.textEnergieWH.setFixedWidth(80,70,50)
        self.textEnergieWH.setReadOnly(True)

        # Integration Time
        self.intPeriod:dataMenu = dataMenu("IntPeriod", "s", parent=self)
        self.intPeriod.setFixedWidth(80,70,50)
        self.intPeriod.setReadOnly(True)

        self.styleElement:str = "background: transparent; background-color: transparent; border: none; color: black;"
        self.textVoltageV.setStyleSheet(self.styleElement)
        self.textCurrentA.setStyleSheet(self.styleElement)
        self.textPowermW.setStyleSheet(self.styleElement)
        self.textEnergieWH.setStyleSheet(self.styleElement)
        self.intPeriod.setStyleSheet(self.styleElement)

        layout = QVBoxLayout()
        layout.setSpacing(0)
        layout.setContentsMargins(0,0,0,0)
        layout.addWidget(label)
        layout.addWidget(self.textVoltageV)
        layout.addWidget(self.textCurrentA)
        layout.addWidget(self.textPowermW)
        layout.addWidget(self.textEnergieWH)
        layout.addWidget(self.intPeriod)
        layout.setSpacing(0)
        #layout.setContentsMargins(1, 1, 1, 1)  # Set the margins inside the frame
        self.setLayout(layout)
        #self.setFixedSize(230, 160)
        self.setStyleSheet("border: 1px solid black;") 

    def update_text_fields(self):
        intPeriods = int(self.ina219.intPeriodS())
        intPeriodm = int(intPeriods/60)
        intPeriods %= 60
        intPeriodh = int(intPeriodm/60)
        intPeriodm %= 60

        self.textVoltageV.setText(f"{self.ina219.voltageV():+.1f}")
        self.textCurrentA.setText(f"{self.ina219.currentA():+.1f}")
        self.textPowermW.setText(f"{self.ina219.powermW():+.1f}"))
        self.textEnergieWH.setText(formatEnergie(self.ina219.energiemWS()/3600/1000))
        self.intPeriod.setText(f"{intPeriodh:2d}:{intPeriodm:2d}:{intPeriods:2d}")

    def getTotalEnergieWh(self):
        return self.ina219.getTotalEnergiemWS()/3600/1000

class MainInaWindow(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)

        # Layout de type grille
        self.main_layout = QGridLayout()
        self.main_layout.setSpacing(0)

        # Add QLabel at the top
        self.TotalEnergieWh:dataMenu = dataMenu(" Total Energie = ", "Wh ", parent=self)
        self.TotalEnergieWh.setReadOnly(True)
        self.TotalEnergieWh.setStyleSheet("border: 1px solid black;") 

        self.TotalEnergieAh:dataMenu = dataMenu(" Total Ah sous 12V = ", "Ah ", parent=self)
        self.TotalEnergieAh.setReadOnly(True)
        self.TotalEnergieAh.setStyleSheet("border: 1px solid black;") 

        self.main_layout.addWidget(self.TotalEnergieWh, 0, 1)
        self.main_layout.addWidget(self.TotalEnergieAh, 0, 2)


        self.widgets = []
        listIna =  AstraIna.getListNames()

        # Calculer le nombre de lignes et de colonnes pour obtenir une forme carrée
        n = len(listIna) + 1
        rows = cols = math.ceil(math.sqrt(n))

        for index, name in enumerate(listIna):
            row = (index // cols) +1 
            col = index % cols
            widget=ina219Frame(AstraIna(name=name), parent=self)
            self.widgets.append(widget)
            self.main_layout.addWidget(widget, row, col)
         
        self.setLayout(self.main_layout)
        self.setWindowTitle('AstrAlimIna')


        # Créer un timer pour mettre à jour tous les widgets toutes les secondes
        self.timer = QTimer()
        self.timer.timeout.connect(lambda: [self.update_text_fields()])
        self.timer.start(1000)  # Met à jour toutes les 1000 millisecondes (1 seconde)
 
    def  update_text_fields(self):
        self.TotalEnergieWh.setText(formatEnergie(self.widgets[1].getTotalEnergieWh()))
        self.TotalEnergieAh.setText(formatEnergie(self.widgets[1].getTotalEnergieWh()/12))
  
        for widget in self.widgets:
            widget.update_text_fields() 

    def closeEvent(self, event):
        AstraIna.exitAll()
        #os.kill(os.getpid(), signal.SIGTERM)

if __name__ == '__main__':
    app = QApplication(sys.argv)
    main_window = MainInaWindow()
    main_window.show()


    sys.exit(app.exec_())

