#!/usr/bin/env python3
import sys
import os
import signal
from PyQt5.QtCore import QTimer, Qt, QRect
from PyQt5.QtWidgets import (QApplication, QWidget, QVBoxLayout, QHBoxLayout, 
                              QLabel, QFrame, QGroupBox, QScrollArea)
from AstraGpio import AstraGpio
from AstraIna import AstraIna
from AstraCommonHmi import dataMenu, AnimatedToggleButton


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


class AlimentationRow(QWidget):
    """Classe de base pour une ligne d'alimentation (DC ou RCA)"""
    def __init__(self, display_name, parent=None):
        super().__init__(parent)
        self.setStyleSheet("""
            AlimentationRow {
                background-color: #1e293b;
                border-radius: 8px;
            }
            AlimentationRow:hover {
                background-color: #334155;
            }
        """)
        
        self.layout = QHBoxLayout(self)
        self.layout.setContentsMargins(15, 8, 15, 8)
        self.layout.setSpacing(15)

        # 1. Zone Contrôle (Toggle ou LED) - Largeur fixe pour alignement
        self.control_widget = QWidget()
        self.control_widget.setFixedWidth(50)
        self.control_layout = QHBoxLayout(self.control_widget)
        self.control_layout.setContentsMargins(0, 0, 0, 0)
        
        # 2. Nom
        self.name_label = QLabel(display_name)
        self.name_label.setStyleSheet("color: #f8fafc; font-size: 11px; font-weight: 600;")
        self.name_label.setFixedWidth(130)
        
        # 3. Status INA
        self.ina_led = QLabel("⬤")
        self.ina_led.setStyleSheet("color: #10b981; font-size: 10px;")
        self.ina_led.setFixedWidth(20)
        
        # 4. Monitoring (Valeurs alignées)
        self.val_v = QLabel("--V")
        self.val_v.setStyleSheet("color: #fbbf24; font-size: 12px; font-weight: 700;")
        self.val_v.setFixedWidth(50)
        self.val_v.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        
        self.val_a = QLabel("--A")
        self.val_a.setStyleSheet("color: #38bdf8; font-size: 12px; font-weight: 700;")
        self.val_a.setFixedWidth(65)
        self.val_a.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        
        self.val_w = QLabel("--W")
        self.val_w.setStyleSheet("color: #f472b6; font-size: 12px; font-weight: 700;")
        self.val_w.setFixedWidth(65)
        self.val_w.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        
        self.val_wh = QLabel("--Wh")
        self.val_wh.setStyleSheet("color: #a78bfa; font-size: 11px;")
        self.val_wh.setFixedWidth(65)
        self.val_wh.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        
        self.val_dur = QLabel("--:--:--")
        self.val_dur.setStyleSheet("color: #64748b; font-size: 10px; font-family: monospace;")
        self.val_dur.setFixedWidth(70)
        self.val_dur.setAlignment(Qt.AlignRight | Qt.AlignVCenter)

        self.layout.addWidget(self.control_widget)
        self.layout.addWidget(self.name_label)
        self.layout.addWidget(self.ina_led)
        self.layout.addWidget(self.val_v)
        self.layout.addWidget(self.val_a)
        self.layout.addWidget(self.val_w)
        self.layout.addWidget(self.val_wh)
        self.layout.addWidget(self.val_dur)
        self.layout.addStretch()

    def update_common(self, ina):
        intPeriods = int(ina.intPeriodS())
        h, m, s = intPeriods // 3600, (intPeriods % 3600) // 60, intPeriods % 60
        
        self.val_v.setText(f"{ina.voltageV():.1f}V")
        self.val_a.setText(f"{ina.currentA():.3f}A")
        self.val_w.setText(f"{ina.powerW():.2f}W")
        self.val_wh.setText(f"{formatEnergie(ina.energieWS()/3600)}Wh")
        self.val_dur.setText(f"{h:d}:{m:02d}:{s:02d}")
        
        color = "#10b981" if ina.getPingOK() else "#ef4444"
        self.ina_led.setStyleSheet(f"color: {color}; font-size: 10px;")


class AlimentationControl(AlimentationRow):
    """Ligne DC avec Toggle"""
    def __init__(self, gpio_name, ina_name, display_name, sub_name, parent=None, size=18):
        super().__init__(display_name, parent)
        self.gpio = AstraGpio(gpio_name)
        self.ina = AstraIna(name=ina_name)
        
        self.toggle = AnimatedToggleButton(self, initial_state=self.gpio.is_on(), 
                                         toggle_callback=self.toggle_action, size=size)
        self.control_layout.addWidget(self.toggle)

    def toggle_action(self, state):
        if state: self.gpio.set_on()
        else: self.gpio.set_off()

    def update_monitoring(self):
        self.update_common(self.ina)
    
    def getTotalEnergieWh(self):
        return self.ina.getTotalEnergiemWS()/3600.0/1000.0

    def updateUI(self):
        self.toggle.updateUI()
        self.update_monitoring()


class AlimentationPwmControl(AlimentationRow):
    """Ligne RCA avec LED statique"""
    def __init__(self, ina_name, display_name, sub_name, parent=None):
        super().__init__(display_name, parent)
        print(f"[DEBUG AlimentationPwmControl] Création pour {display_name} avec ina_name='{ina_name}'")
        try:
            self.ina = AstraIna(name=ina_name)
            print(f"[DEBUG AlimentationPwmControl] {display_name}: INA créé, configured={self.ina.configured}, pingOK={self.ina.getPingOK()}")
        except Exception as e:
            print(f"[DEBUG AlimentationPwmControl] {display_name}: ERREUR lors de la création de l'INA: {e}")
            raise
        
        self.status_led = QLabel("⬤")
        self.status_led.setStyleSheet("color: #475569; font-size: 14px;")
        self.control_layout.addWidget(self.status_led, alignment=Qt.AlignCenter)

    def update_monitoring(self):
        self.update_common(self.ina)
        power = self.ina.powerW()
        voltage = self.ina.voltageV()
        current = self.ina.currentA()
        ping_ok = self.ina.getPingOK()
        
        # Debug périodique (toutes les 5 secondes environ)
        if not hasattr(self, '_debug_counter'):
            self._debug_counter = 0
        self._debug_counter += 1
        if self._debug_counter % 5 == 0:
            print(f"[DEBUG AlimentationPwmControl] {self.name_label.text()}: V={voltage:.2f}V, A={current:.3f}A, W={power:.2f}W, pingOK={ping_ok}")
        
        color = "#10b981" if power > 0.1 else "#475569"
        self.status_led.setStyleSheet(f"color: {color}; font-size: 14px;")
    
    def getTotalEnergieWh(self):
        return self.ina.getTotalEnergiemWS()/3600.0/1000.0

    def updateUI(self):
        self.update_monitoring()


class MainAlimWindow(QWidget):
    def __init__(self, parent=None, size=18):
        super().__init__(parent)
        self.tensionRefV = 12
        self.initUI(size)
        
    def initUI(self, size):
        self.setStyleSheet("""
            MainAlimWindow {
                background-color: #0f172a;
            }
            QScrollArea {
                border: none;
                background-color: #0f172a;
            }
            #container {
                background-color: #0f172a;
            }
            .section-title {
                color: #64748b;
                font-size: 10px;
                font-weight: 800;
                text-transform: uppercase;
                letter-spacing: 1px;
                margin-top: 10px;
                margin-left: 5px;
            }
        """)
        
        main_layout = QVBoxLayout(self)
        main_layout.setContentsMargins(15, 15, 15, 15)
        main_layout.setSpacing(10)

        # --- HEADER (Total) ---
        header = QHBoxLayout()
        header.setSpacing(20)
        
        def create_total_card(title, color):
            card = QFrame()
            card.setStyleSheet(f"background-color: #1e293b; border-radius: 10px; border: 1px solid #334155;")
            card.setFixedWidth(140)
            l = QVBoxLayout(card)
            l.setContentsMargins(12, 8, 12, 8)
            l.setSpacing(2)
            t = QLabel(title)
            t.setStyleSheet("color: #94a3b8; font-size: 9px; font-weight: 700; text-transform: uppercase;")
            v = QLabel("--")
            v.setStyleSheet(f"color: {color}; font-size: 16px; font-weight: 800;")
            l.addWidget(t)
            l.addWidget(v)
            return card, v

        self.card_wh, self.lbl_total_wh = create_total_card("Énergie Totale", "#fbbf24")
        self.card_ah, self.lbl_total_ah = create_total_card("Total Ah (12V)", "#38bdf8")
        
        header.addWidget(self.card_wh)
        header.addWidget(self.card_ah)
        header.addStretch()
        main_layout.addLayout(header)

        # --- CONTENT ---
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        container = QWidget()
        container.setObjectName("container")
        content_layout = QVBoxLayout(container)
        content_layout.setContentsMargins(0, 0, 0, 0)
        content_layout.setSpacing(5)

        # DC Section
        t_dc = QLabel("Lignes Directes (DC)")
        t_dc.setProperty("class", "section-title")
        content_layout.addWidget(t_dc)
        
        self.widgets = []
        for cfg in [("AstraDc1", "AstraDc1", "Sortie 12V #1", "DC1"),
                    ("AstraDc2", "AstraDc2", "Sortie 12V #2", "DC2"),
                    ("AstraDc3", "AstraDc3", "Sortie Aux #3", "DC3")]:
            w = AlimentationControl(*cfg, parent=self, size=size)
            self.widgets.append(w)
            content_layout.addWidget(w)

        # RCA Section
        t_rca = QLabel("Bandes Chauffantes (RCA)")
        t_rca.setProperty("class", "section-title")
        content_layout.addWidget(t_rca)
        
        # CORRECTION: Inversion pour correspondre au mapping réel
        # AstraPwm1 contrôle en fait l'INA 0x4d (AstraPwm2) et vice versa
        for cfg in [("AstraPwm2", "Ligne RCA #1", "PWM1"),  # Inversé : AstraPwm2 lit 0x4d qui correspond à RCA #1
                    ("AstraPwm1", "Ligne RCA #2", "PWM2")]:  # Inversé : AstraPwm1 lit 0x49 qui correspond à RCA #2
            w = AlimentationPwmControl(*cfg, parent=self)
            self.widgets.append(w)
            content_layout.addWidget(w)

        content_layout.addStretch()
        scroll.setWidget(container)
        main_layout.addWidget(scroll)

        # Timer
        self.timer = QTimer()
        self.timer.timeout.connect(self.update_all)
        self.timer.start(1000)

    def update_all(self):
        if self.widgets:
            total_wh = sum(w.getTotalEnergieWh() for w in self.widgets)
            self.lbl_total_wh.setText(f"{formatEnergie(total_wh)} Wh")
            self.lbl_total_ah.setText(f"{formatEnergie(total_wh / self.tensionRefV)} Ah")
        for w in self.widgets:
            w.update_monitoring()

    def updateUI(self):
        for w in self.widgets: w.updateUI()

    def closeEvent(self, event):
        AstraIna.exitAll()

if __name__ == '__main__':
    app = QApplication(sys.argv)
    main = MainAlimWindow()
    main.show()
    sys.exit(app.exec_())
