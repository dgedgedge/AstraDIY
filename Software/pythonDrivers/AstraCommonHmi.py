#!/usr/bin/env python3
import sys
import os
import signal
from PyQt5.QtCore import QTimer
from PyQt5.QtWidgets import QApplication, QMainWindow, QWidget, QPushButton, QVBoxLayout, QFrame
from PyQt5.QtWidgets import QHBoxLayout, QLineEdit, QLabel, QFrame, QComboBox
from PyQt5.QtCore import Qt, QPropertyAnimation, QRect



class dataMenu(QWidget):
    def __init__(self, label, unit, readOnly=True, parent=None):
        super().__init__(parent)
        self.unit=unit
        self.label=label
        self.readOnly=readOnly
        self.dataAvailable=False
        self.height=40

        self.type_label = QLabel(self.label, self)  
        self.type_label.setAlignment(Qt.AlignCenter)
        self.type_label.adjustSize()
        self.type_label.setFixedHeight(self.height)

        self.line_edit = QLineEdit(self)
        self.line_edit.adjustSize()
        #self.line_edit.setInputMask('9999')  # Limite les caractères à des chiffres uniquement
        self.line_edit.setFixedHeight(self.height)
        self.line_edit.setAlignment(Qt.AlignRight)

        self.unit_label = QLabel(self.unit, self)  
        self.unit_label.setAlignment(Qt.AlignCenter)
        self.type_label.setFixedHeight(self.height)
        self.unit_label.adjustSize()
        
        self.type_label.setStyleSheet("border: none;")
        self.unit_label.setStyleSheet("border: none;")  
        #self.unit_label.setFixedHeight(50)

        # Mettre le QLabel et le QLineEdit dans un QHBoxLayout pour les aligner horizontalement
        layout = QHBoxLayout()
        layout.addWidget(self.type_label)
        layout.addWidget(self.line_edit)
        layout.addWidget(self.unit_label)
        layout.setSpacing(0)
        layout.setContentsMargins(0, 0, 0, 0)  # Set the margins inside the frame

        # Frame pour les assembler
        self.subWindow = QFrame()
        self.subWindow.setFrameShape(QFrame.Box)
        self.subWindow.setFrameShadow(QFrame.Raised)
        self.subWindow.setLayout(layout)
        
        self.mainLayout = QVBoxLayout()
        self.mainLayout.setSpacing(0)
        self.mainLayout.setContentsMargins(0,0,0,0)

        self.mainLayout.addWidget(self.subWindow)
        self.setLayout(self.mainLayout)
        self.adjustSize()
        self.restaureStyleSheet()
        
    def setText(self, value):
        self.setDataAvailable(True)
        self.line_edit.setText(value)

    def setInputMask(self, value):
        self.line_edit.setInputMask(value)

    def getText(self):
        return self.line_edit.text()

    def setFixedWidth(self, w1, w2, w3):
        self.type_label.setFixedWidth(w1)
        self.line_edit.setFixedWidth(w2)
        self.unit_label.setFixedWidth(w3)
    

    def restaureStyleSheet(self):
        if self.readOnly:
            if self.dataAvailable:
                self.line_edit.setStyleSheet("""
                    QLineEdit {
                        background: transparent;
                        background-color: transparent;
                        border: none;
                        color: #1f2937;
                    }
                """)
            else:
                self.line_edit.setText(f"NAvail")
                self.line_edit.setStyleSheet("""
                    QLineEdit {
                        background: transparent;
                        background-color: #fee2e2;
                        border: 1px solid #fca5a5;
                        border-radius: 4px;
                        color: #991b1b;
                    }
                """)
        else:
            self.line_edit.setStyleSheet("""
                QLineEdit {
                    border: 1px solid #d1d5db;
                    border-radius: 5px;
                    background-color: #ffffff;
                    color: #1f2937;
                }
            """)
            
    def setReadOnly(self, readOnly):
        self.readOnly=readOnly
        self.line_edit.setReadOnly(self.readOnly)
        self.restaureStyleSheet()

    def setDataAvailable(self, dataAvailable=True):
        if not (self.dataAvailable == dataAvailable):
            self.dataAvailable=dataAvailable
            self.restaureStyleSheet()

    def connect(self, doSomething):
        self.line_edit.textEdited.connect(doSomething)

class AnimatedToggleButton(QWidget):
    def __init__(self, parent=None, initial_state=False, toggle_callback=None, size=30):
        super().__init__(parent)
        self.state=initial_state
        self.size=size
        self.radius=str(int(size/2))
        self.setFixedSize(self.size*2, self.size)
        self.toggle_callback = None
        
        # Background
        self.background = QPushButton('', self)
        self.background.setFixedSize(self.size*2, self.size)
        self.background.setStyleSheet("background-color: #d1d5db; border-radius: "+self.radius+"px; border: 1px solid #9ca3af;")
        self.background.setCheckable(True)
        self.background.clicked.connect(self._toggle)
        
        # Slider
        self.slider = QPushButton('', self)
        self.slider.setFixedSize(self.size, self.size)
        self.slider.setStyleSheet("background-color: #ffffff; border-radius: "+self.radius+"px; border: 1px solid #e5e7eb; font-size: 8px; font-weight: bold;")
        self.slider.setEnabled(True)
        self.slider.clicked.connect(self._toggle)

        # Animation
        self.animation = QPropertyAnimation(self.slider, b"geometry")
        self.animation.setDuration(250)  # Duration of the animation in milliseconds

        # Set initial state
        self.background.setChecked(initial_state)
        self.slider.setChecked(initial_state)
        if initial_state:
            self.slider.setGeometry(0, 0, self.size, self.size)
        else:
            self.slider.setGeometry(self.size, 0, self.size, self.size)
        self._updateUI(self.state)
        self.toggle_callback = toggle_callback  # Save the callback function


    def _toggle(self):
        self.state = not self.state
        self._updateUIAndCallback(self.state)
        
    def _updateUI(self, state):
        if state:
            self.animation.setStartValue(QRect(0, 0, self.size, self.size))
            self.animation.setEndValue(QRect(self.size, 0, self.size, self.size))
            self.background.setStyleSheet("background-color: #10b981; border-radius: "+self.radius+"px; border: 1px solid #059669;")
            self.slider.setStyleSheet("background-color: #ffffff; border-radius: "+self.radius+"px; border: 1px solid #e5e7eb; font-size: 7px; font-weight: 600; color: #10b981;")
            self.slider.setText('ON')
        else:
            self.animation.setStartValue(QRect(self.size, 0, self.size, self.size))
            self.animation.setEndValue(QRect(0, 0, self.size, self.size))
            self.background.setStyleSheet("background-color: #ef4444; border-radius: "+self.radius+"px; border: 1px solid #dc2626;")
            self.slider.setStyleSheet("background-color: #ffffff; border-radius: "+self.radius+"px; border: 1px solid #e5e7eb; font-size: 7px; font-weight: 600; color: #ef4444;")
            self.slider.setText('OFF')
        self.animation.start()

    def _updateUIAndCallback(self, state):
        self._updateUI(state)

        # Call the callback function if provided
        if self.toggle_callback:
            self.toggle_callback(state)
            
    def isChecked(self):
        return self.state

    def setState(self, state):
        if not (self.state == state):
            self.state=state
            self._updateUIAndCallback(state)
        else:
            self._updateUI(state)

    def updateUI(self):
        # self._updateUI(self.state)
        if not self.state:
            self.slider.setGeometry(0, 0, self.size, self.size)
        else:
            self.slider.setGeometry(self.size, 0, self.size, self.size)

if __name__ == '__main__':
    app = QApplication(sys.argv)
    window = QWidget()

    data_menu = dataMenu('toto 1', 'ms', parent=window)
    data_menu.setText("tutu")
    rodata_menu = dataMenu('toto 2', 'ms', parent=window)
    rodata_menu.setReadOnly(True)
    rodata_menu.setText("ReadOnly")
    
    def on_toggle(state):
        print(f'Toggle state changed to: {"On" if state else "Off"}')
        rodata_menu.setReadOnly(state)

    def on_toggleAvail(state):
        print(f'Toggle state changed to: {"On Available" if state else "Off"}')
        if state:
            data_menu.setText("Avail")
        else:
            data_menu.setDataAvailable(False)
    #rodata_menu.setFixedWidth(80,70,50)

        

 
    # Création du bouton de bascule animé avec un état initial
    animated_toggle_button1 = AnimatedToggleButton(parent=window, initial_state=False, toggle_callback=on_toggle)
    animated_toggle_button2 = AnimatedToggleButton(parent=window, initial_state=True, toggle_callback=on_toggleAvail)

    # Layout
    layout = QVBoxLayout()
    layout.addWidget(animated_toggle_button1)
    layout.addWidget(animated_toggle_button2)
    layout.addWidget(data_menu)
    layout.addWidget(rodata_menu)
    window.setLayout(layout)

    

       
    # Configuration de la fenêtre principale
    window.setWindowTitle('Animated Toggle Button and data menue')
    window.resize(200, 100)
    window.show()
    
    sys.exit(app.exec_())

