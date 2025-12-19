#!/usr/bin/env python3
import sys
import os
import signal
from PyQt5.QtCore import QTimer
from PyQt5.QtWidgets import QApplication, QMainWindow, QWidget, QPushButton, QVBoxLayout
from PyQt5.QtWidgets import QHBoxLayout, QLineEdit, QLabel, QFrame, QComboBox
from PyQt5.QtCore import Qt
from AstraGpio import AstraGpio
from AstraCommonHmi import dataMenu, AnimatedToggleButton


class GpioControl(QWidget):
    def __init__(self, name, parent=None, size=30):
        super().__init__(parent)
        self.name=name
        self.gpio = AstraGpio(name)
        #self.setStyleSheet("border: 1px solid black;") 
        
        # Zone de texte initiale
        self.TextName = QLabel(self.name, self)
        self.TextName.setFixedWidth(100)

        # Bouton On/Off
        self.toggle_button = AnimatedToggleButton(self, initial_state=self.gpio.is_on(), toggle_callback=self.toggle_action, size=size)

        # Layout
        layout = QHBoxLayout()
        layout.setSpacing(0)
        layout.addWidget(self.TextName)
        layout.addWidget(self.toggle_button)
        layout.setAlignment(Qt.AlignTop | Qt.AlignLeft)
        self.setLayout(layout)
        self.adjustSize()

    def toggle_action(self, state):
        if state:
            self.gpio.set_on()
        else:
            self.gpio.set_off()
        # self.gpio.print_status()

    def updateUI(self):
        self.toggle_button.updateUI()

class MainGpioWindow(QWidget):
    def __init__(self, parent=None, size=30):
        super().__init__(parent)
        self.main_layout = QVBoxLayout()
        self.main_layout.setSpacing(0)

        self.widgets = []
        for name in ["AstraDc1", "AstraDc2", "AstraDc3"]:
            wiget=GpioControl(name, size=size)
            self.widgets.append(wiget)
            self.main_layout.addWidget(wiget)
        
        self.setLayout(self.main_layout)
        self.setWindowTitle('AstrAlimGpio')

    def updateUI(self):
        for widget in self.widgets:
            widget.updateUI()
            
if __name__ == '__main__':
    app = QApplication(sys.argv)
    main_window = MainGpioWindow()
    main_window.show()


    sys.exit(app.exec_())

