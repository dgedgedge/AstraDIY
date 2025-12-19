#!/usr/bin/env python3
import sys
import os
import signal
from AstraAlimHmi import MainAlimWindow
from AstraPwmHmi import MainPwmWindow
from AstraGpsHmi import MainGpsWindow

from PyQt5.QtWidgets import QApplication, QMainWindow, QWidget, QTabWidget, QVBoxLayout, QLabel

QApplication.setApplicationName("AstraDIY")
QApplication.setDesktopFileName("AstraDIY.desktop")

# Main window class
class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()

        # Create a QTabWidget
        self.tabs = QTabWidget()
        
        # Create instances of the widgets
        self.pwm_hmi = MainPwmWindow()
        self.alim_hmi = MainAlimWindow(size=18)
        self.gps_hmi = MainGpsWindow()
        
        # Add widgets as tabs
        self.tabs.addTab(self.pwm_hmi, "☀️ Bandes Chauffantes")
        self.tabs.addTab(self.alim_hmi, "⚡ Alimentations")
        self.tabs.addTab(self.gps_hmi, "🗺️ GPS et Horloge")
        
        # Set the central widget of the main window to be the QTabWidget
        self.setCentralWidget(self.tabs)
        
        # Window settings
        self.setWindowTitle('Astralim Panneau de Contrôle')
        
        # Définir une taille minimale pour la fenêtre
        self.setMinimumSize(1000, 700)
        
        # Taille par défaut
        self.resize(1400, 900)

    def closeEvent(self, event):
        # Arrêter proprement tous les composants des fenêtres enfants
        # MainPwmWindow
        if hasattr(self.pwm_hmi, 'closeEvent'):
            # Créer un événement factice pour déclencher le closeEvent
            from PyQt5.QtGui import QCloseEvent
            fake_event = QCloseEvent()
            self.pwm_hmi.closeEvent(fake_event)
        
        # MainAlimWindow
        if hasattr(self.alim_hmi, 'closeEvent'):
            from PyQt5.QtGui import QCloseEvent
            fake_event = QCloseEvent()
            self.alim_hmi.closeEvent(fake_event)
        
        # MainGpsWindow
        if hasattr(self.gps_hmi, 'closeEvent'):
            from PyQt5.QtGui import QCloseEvent
            fake_event = QCloseEvent()
            self.gps_hmi.closeEvent(fake_event)
        
        event.accept()
        

if __name__ == '__main__':
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec_())
