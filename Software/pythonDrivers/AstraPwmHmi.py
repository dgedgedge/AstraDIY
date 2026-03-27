#!/usr/bin/env python3
import sys
import os
import signal
from PyQt5.QtCore import QTimer, Qt
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QPushButton, QVBoxLayout,
    QHBoxLayout, QLineEdit, QLabel, QFrame, QComboBox, QGroupBox, 
    QFormLayout, QScrollArea, QSpinBox, QDoubleSpinBox, QSplitter, QSlider,
    QButtonGroup, QStackedWidget, QMessageBox, QDialog
)
from AstraPwm import AstraPwm
from AstraCommonHmi import dataMenu, AnimatedToggleButton


class PIDConfigDialogShared(QDialog):
    def __init__(self, parent, widgets):
        super().__init__(parent)
        self.widgets = widgets
        self.reference_astra_drew = widgets[0].AstraDrew if widgets else None
        self.initUI()
    
    def initUI(self):
        self.setWindowTitle("Réglages PID")
        self.setModal(True)
        self.setMinimumWidth(400)
        self.setStyleSheet("""
            QDialog { 
                background-color: #0f172a; 
                border: 1px solid #334155;
            }
            QLabel { color: #94a3b8; font-weight: 700; font-size: 10px; text-transform: uppercase; letter-spacing: 0.5px; }
            QDoubleSpinBox { 
                background-color: #1e293b; 
                color: #f8fafc; 
                border: 1px solid #334155; 
                padding: 8px; 
                border-radius: 6px; 
                font-size: 13px;
                font-weight: 600;
            }
            QDoubleSpinBox:disabled { 
                background-color: #0f172a; 
                color: #475569; 
                border: 1px solid #1e293b; 
            }
            QPushButton#action { 
                padding: 10px; 
                border-radius: 6px; 
                font-weight: 800; 
                font-size: 11px;
                text-transform: uppercase;
            }
        """)
        
        layout = QVBoxLayout(self)
        layout.setSpacing(25)
        layout.setContentsMargins(30, 30, 30, 30)
        
        title = QLabel("Paramètres PID")
        title.setStyleSheet("font-size: 22px; font-weight: 900; color: #f8fafc; text-transform: none; letter-spacing: 0px;")
        layout.addWidget(title)
        
        form = QFormLayout()
        form.setSpacing(15)
        form.setLabelAlignment(Qt.AlignLeft)
        
        def create_spin():
            s = QDoubleSpinBox()
            s.setRange(0.0, 100.0)
            s.setDecimals(3)
            return s

        self.kp_spin = create_spin()
        if self.reference_astra_drew: self.kp_spin.setValue(self.reference_astra_drew.get_Kp())
        
        self.ki_spin = create_spin()
        if self.reference_astra_drew: self.ki_spin.setValue(self.reference_astra_drew.get_Ki())
        
        self.kd_spin = create_spin()
        if self.reference_astra_drew: self.kd_spin.setValue(self.reference_astra_drew.get_Kd())
        
        form.addRow("Kp (Proportionnel)", self.kp_spin)
        form.addRow("Ki (Intégral)", self.ki_spin)
        form.addRow("Kd (Dérivé)", self.kd_spin)
        layout.addLayout(form)
        
        # Switch Auto-ajustement avec label
        auto_layout = QHBoxLayout()
        auto_layout.setSpacing(15)
        auto_label = QLabel("Auto-ajustement Intelligent")
        auto_label.setStyleSheet("color: #94a3b8; font-weight: 700; font-size: 12px;")
        
        initial_auto_state = False
        if self.reference_astra_drew: 
            initial_auto_state = self.reference_astra_drew.get_autoUpdateKpKiKd()
        
        self.auto_toggle = AnimatedToggleButton(self, initial_state=initial_auto_state, 
                                                toggle_callback=self.on_auto_toggle_changed)
        auto_layout.addWidget(auto_label)
        auto_layout.addStretch()
        auto_layout.addWidget(self.auto_toggle)
        layout.addLayout(auto_layout)
        
        # Initialiser l'état des spinboxes selon l'état initial
        self.update_spinboxes_state(initial_auto_state)
        
        btns = QHBoxLayout()
        btns.setSpacing(10)
        
        reset = QPushButton("Réinitialiser")
        reset.setObjectName("action")
        reset.setStyleSheet("background-color: transparent; color: #f59e0b; border: 1px solid #f59e0b;")
        reset.clicked.connect(self.reset_to_defaults)
        
        cancel = QPushButton("Annuler")
        cancel.setObjectName("action")
        cancel.setStyleSheet("background-color: #334155; color: white; border: none;")
        cancel.clicked.connect(self.reject)
        
        save = QPushButton("Enregistrer")
        save.setObjectName("action")
        save.setStyleSheet("background-color: #10b981; color: white; border: none;")
        save.clicked.connect(self.accept_config)
        
        btns.addWidget(reset); btns.addStretch(); btns.addWidget(cancel); btns.addWidget(save)
        layout.addLayout(btns)

    def on_auto_toggle_changed(self, state):
        """Appelé quand le switch auto-ajustement change d'état"""
        self.update_spinboxes_state(state)
    
    def update_spinboxes_state(self, auto_enabled):
        """Met à jour l'état (activé/désactivé) des spinboxes selon l'état de l'auto-ajustement"""
        self.kp_spin.setEnabled(not auto_enabled)
        self.ki_spin.setEnabled(not auto_enabled)
        self.kd_spin.setEnabled(not auto_enabled)

    def reset_to_defaults(self):
        self.kp_spin.setValue(2.0)
        self.ki_spin.setValue(0.0)
        self.kd_spin.setValue(0.0)
        self.auto_toggle.setState(False)
        self.update_spinboxes_state(False)

    def accept_config(self):
        kp, ki, kd = self.kp_spin.value(), self.ki_spin.value(), self.kd_spin.value()
        auto = self.auto_toggle.isChecked()
        for w in self.widgets:
            w.AstraDrew.Kp, w.AstraDrew.Ki, w.AstraDrew.Kd = kp, ki, kd
            if auto: w.AstraDrew.set_autoUpdateKpKiKd()
            else: w.AstraDrew.unset_autoUpdateKpKiKd()
            w.AstraDrew.save()
        self.accept()


class SensorConfigDialog(QDialog):
    def __init__(self, parent, widgets):
        super().__init__(parent)
        self.widgets = widgets
        self.sensor_combos = []
        self.initUI()
        
    def initUI(self):
        self.setWindowTitle("Configuration Capteurs")
        self.setModal(True)
        self.setMinimumWidth(420)
        self.setStyleSheet("""
            QDialog { 
                background-color: #0f172a; 
                border: 1px solid #334155;
            }
            QLabel#header { color: #94a3b8; font-weight: 800; font-size: 10px; text-transform: uppercase; letter-spacing: 1px; }
            QComboBox { 
                background-color: #1e293b; 
                color: #f8fafc; 
                border: 1px solid #334155; 
                padding: 12px; 
                border-radius: 8px; 
                font-size: 14px;
                font-family: 'JetBrains Mono', monospace;
            }
            QComboBox::drop-down { border: none; }
            QPushButton#action { 
                padding: 12px; 
                border-radius: 6px; 
                font-weight: 800; 
                font-size: 11px;
                text-transform: uppercase;
            }
        """)
        
        layout = QVBoxLayout(self)
        layout.setSpacing(30)
        layout.setContentsMargins(35, 35, 35, 35)
        
        title = QLabel("Assignation des Capteurs")
        title.setStyleSheet("font-size: 22px; font-weight: 900; color: #f8fafc;")
        layout.addWidget(title)
        
        available_sensors = self.widgets[0].AstraDrew.get_listTemp() if self.widgets else []
        
        sensors_lay = QVBoxLayout()
        sensors_lay.setSpacing(20)
        
        for idx, widget in enumerate(self.widgets):
            v_box = QVBoxLayout()
            v_box.setSpacing(8)
            lbl = QLabel(f"LIGNE RCA #{idx + 1}")
            lbl.setObjectName("header")
            combo = QComboBox()
            combo.addItem("— Aucun capteur assigné")
            for s in available_sensors: combo.addItem(s)
            
            curr = widget.AstraDrew.get_associateTemp()
            if curr:
                idx_found = combo.findText(curr)
                if idx_found >= 0: combo.setCurrentIndex(idx_found)
                else: combo.addItem(curr); combo.setCurrentIndex(combo.count()-1)
            
            self.sensor_combos.append(combo)
            v_box.addWidget(lbl); v_box.addWidget(combo)
            sensors_lay.addLayout(v_box)
        
        layout.addLayout(sensors_lay)
        
        invert = QPushButton("⇅  Inverser les Assignations")
        invert.setObjectName("action")
        invert.setStyleSheet("""
            QPushButton { 
                background-color: #1e293b; 
                color: #f59e0b; 
                border: 1px solid #475569; 
                font-size: 12px;
                letter-spacing: 0.5px;
            }
            QPushButton:hover { background-color: #334155; }
        """)
        invert.clicked.connect(self.invert_sensors)
        layout.addWidget(invert)
        
        btns = QHBoxLayout()
        btns.setSpacing(12)
        btns.addStretch()
        
        cancel = QPushButton("Annuler")
        cancel.setObjectName("action")
        cancel.setMinimumWidth(100)
        cancel.setStyleSheet("background-color: #334155; color: white; border: none;")
        cancel.clicked.connect(self.reject)
        
        apply = QPushButton("Confirmer")
        apply.setObjectName("action")
        apply.setMinimumWidth(120)
        apply.setStyleSheet("background-color: #10b981; color: white; border: none;")
        apply.clicked.connect(self.accept_config)
        
        btns.addWidget(cancel); btns.addWidget(apply)
        layout.addLayout(btns)

    def invert_sensors(self):
        if len(self.sensor_combos) >= 2:
            i1, i2 = self.sensor_combos[0].currentIndex(), self.sensor_combos[1].currentIndex()
            self.sensor_combos[0].setCurrentIndex(i2); self.sensor_combos[1].setCurrentIndex(i1)

    def accept_config(self):
        for idx, w in enumerate(self.widgets):
            s = self.sensor_combos[idx].currentText()
            w.AstraDrew.set_associateTemp(None if "— Aucun" in s else s)
            w.AstraDrew.save()
        self.accept()


class DrewControl(QWidget):
    def __init__(self, name, parent=None):
        super().__init__(parent)
        self.name = name
        self.display_name = "RCA #1" if name == "AstraPwm1" else "RCA #2"
        self.AstraDrew = AstraPwm(name)
        self.buttonAsservOn = False
        self.buttonRoseeConsigneOn = False
        self.initUI()

    def initUI(self):
        self.setStyleSheet("""
            DrewControl { background-color: #1e293b; border-radius: 12px; }
            QLabel#title { color: #f8fafc; font-size: 15px; font-weight: 800; }
            QLabel#label { color: #64748b; font-size: 9px; font-weight: 800; text-transform: uppercase; }
            QLabel#value { font-size: 13px; font-weight: 800; }
            
            QSlider::groove:horizontal { 
                border: 1px solid #334155; 
                height: 10px; 
                background: #0f172a; 
                margin: 2px 0; 
                border-radius: 5px;
            }
            QSlider::handle:horizontal { 
                background: #3b82f6; 
                border: 1px solid #2563eb; 
                width: 24px; 
                height: 24px; 
                margin: -8px 0; 
                border-radius: 12px;
            }
            QSlider::sub-page:horizontal { background: #3b82f6; border-radius: 5px; }
        """)
        
        main_layout = QHBoxLayout(self)
        main_layout.setContentsMargins(20, 15, 20, 15)
        main_layout.setSpacing(25)

        # --- GAUCHE : IDENTITÉ ---
        left_panel = QVBoxLayout()
        title = QLabel(self.display_name, self); title.setObjectName("title")
        left_panel.addWidget(title)
        
        self.mode_group = QButtonGroup(self)
        modes_lay = QHBoxLayout()
        modes_lay.setSpacing(2)
        for i, (txt, tip) in enumerate([("PUISSANCE", "Manuel"), ("TEMPÉRATURE", "PID Fixe"), ("AUTO", "Auto Rosée")]):
            btn = QPushButton(txt)
            btn.setCheckable(True); btn.setToolTip(tip); btn.setMinimumWidth(60)
            btn.setStyleSheet("""
                QPushButton { background-color: #334155; color: #94a3b8; border: none; font-size: 9px; font-weight: 800; padding: 6px; }
                QPushButton:checked { background-color: #3b82f6; color: white; }
                QPushButton:first { border-top-left-radius: 6px; border-bottom-left-radius: 6px; }
                QPushButton:last { border-top-right-radius: 6px; border-bottom-right-radius: 6px; }
            """)
            self.mode_group.addButton(btn, i); modes_lay.addWidget(btn)
        self.mode_group.buttonClicked.connect(self.on_mode_changed)
        self.mode_group.button(0).setChecked(True)
        left_panel.addLayout(modes_lay); left_panel.addStretch()
        main_layout.addLayout(left_panel)

        # --- CENTRE : SLIDERS ---
        center_panel = QVBoxLayout()
        center_panel.setSpacing(10)
        
        # Ligne Puissance
        self.p_row_widget = QWidget()
        p_row = QHBoxLayout(self.p_row_widget)
        p_row.setContentsMargins(0, 0, 0, 0)
        p_lbl = QLabel("PUISSANCE", self); p_lbl.setObjectName("label"); p_lbl.setFixedWidth(70)
        self.p_slider = QSlider(Qt.Horizontal); self.p_slider.setRange(0, 100)
        self.p_slider.valueChanged.connect(self.set_power)
        self.p_val = QLabel("0%", self); self.p_val.setObjectName("value"); self.p_val.setFixedWidth(45)
        self.p_val.setStyleSheet("color: #38bdf8;")
        p_row.addWidget(p_lbl); p_row.addWidget(self.p_slider); p_row.addWidget(self.p_val)
        
        # Ligne Consigne
        self.c_row_widget = QWidget()
        c_row = QHBoxLayout(self.c_row_widget)
        c_row.setContentsMargins(0, 0, 0, 0)
        c_lbl = QLabel("CONSIGNE", self); c_lbl.setObjectName("label"); c_lbl.setFixedWidth(70)
        self.c_slider = QSlider(Qt.Horizontal); self.c_slider.setRange(-100, 300)
        self.c_slider.valueChanged.connect(self.set_consigne)
        self.c_val = QLabel("10.0°C", self); self.c_val.setObjectName("value"); self.c_val.setFixedWidth(55)
        self.c_val.setStyleSheet("color: #fbbf24;")
        c_row.addWidget(c_lbl); c_row.addWidget(self.c_slider); c_row.addWidget(self.c_val)
        
        center_panel.addWidget(self.p_row_widget); center_panel.addWidget(self.c_row_widget)
        main_layout.addLayout(center_panel, stretch=1)

        # --- DROITE : MONITORING ---
        right_panel = QHBoxLayout()
        right_panel.setSpacing(15)
        
        def add_mon(title, color):
            v = QVBoxLayout(); v.setSpacing(2); v.setAlignment(Qt.AlignCenter)
            l = QLabel(title); l.setObjectName("label")
            val = QLabel("--"); val.setObjectName("value"); val.setStyleSheet(f"color: {color};")
            v.addWidget(l); v.addWidget(val); return v, val

        v1, self.lbl_temp = add_mon("TEMP RCA", "#10b981")
        v2, self.lbl_amb = add_mon("AMBIANT", "#94a3b8")
        v3, self.lbl_hum = add_mon("HUMIDITÉ", "#38bdf8")
        v4, self.lbl_dew = add_mon("ROSÉE", "#a78bfa")
        
        self.cfg_btn = QPushButton("⚙"); self.cfg_btn.setFixedSize(28, 28)
        self.cfg_btn.setStyleSheet("background: transparent; color: #475569; font-size: 18px; font-weight: 400;")
        self.cfg_btn.clicked.connect(self.open_config_dialog)
        
        right_panel.addLayout(v1); right_panel.addLayout(v2)
        right_panel.addLayout(v3); right_panel.addLayout(v4)
        right_panel.addSpacing(5); right_panel.addWidget(self.cfg_btn)
        main_layout.addLayout(right_panel)

        # Init states
        self.on_mode_changed(self.mode_group.button(0))
        self.update_text_fields()

    def on_mode_changed(self, btn):
        try:
            mid = self.mode_group.id(btn)
            print(f"[DEBUG on_mode_changed] {self.name}: mode_id={mid} (0=Manu, 1=PID, 2=Auto)")
            
            # Visibilité des sliders selon le mode
            self.p_row_widget.setVisible(mid == 0 or mid == 2) # Visible en MANU et AUTO (monitoring)
            self.c_row_widget.setVisible(mid == 1 or mid == 2) # Visible en PID et AUTO
            
            self.p_slider.setEnabled(mid == 0); self.c_slider.setEnabled(mid == 1)
            
            if mid == 0: # Manu
                print(f"[DEBUG on_mode_changed] {self.name}: Mode MANUEL activé")
                self.p_row_widget.setVisible(True)
                self.c_row_widget.setVisible(False)
                if self.AstraDrew.isAserv(): self.AstraDrew.stopAserv()
                self.buttonAsservOn = False; self.buttonRoseeConsigneOn = False
            elif mid == 1: # PID Fixe
                print(f"[DEBUG on_mode_changed] {self.name}: Mode PID activé - vérification du capteur...")
                self.p_row_widget.setVisible(False)
                self.c_row_widget.setVisible(True)
                temp_sensor = self.AstraDrew.get_associateTemp()
                print(f"[DEBUG on_mode_changed] {self.name}: temp_sensor='{temp_sensor}' (type={type(temp_sensor)})")
                print(f"[DEBUG on_mode_changed] {self.name}: not temp_sensor={not temp_sensor}, ==''={temp_sensor == ''}, =='Unset Temp Sensor'={temp_sensor == 'Unset Temp Sensor'}")
                
                # Autoconfiguration : si aucun capteur n'est configuré, on essaie d'en trouver un disponible
                if not temp_sensor or temp_sensor == "" or temp_sensor == "Unset Temp Sensor":
                    print(f"[DEBUG on_mode_changed] {self.name}: ⚠ Capteur non configuré, recherche d'un capteur disponible...")
                    available_sensors = self.AstraDrew.get_listTemp()
                    if available_sensors and len(available_sensors) > 0:
                        # Auto-configuration avec le premier capteur disponible
                        auto_sensor = available_sensors[0]
                        print(f"[DEBUG on_mode_changed] {self.name}: ✓ Autoconfiguration avec '{auto_sensor}'")
                        self.AstraDrew.set_associateTemp(auto_sensor)
                        self.AstraDrew.save()  # Sauvegarder la configuration automatique
                        temp_sensor = auto_sensor
                        print(f"[DEBUG on_mode_changed] {self.name}: ✓ Capteur auto-configuré et sauvegardé")
                    else:
                        print(f"[DEBUG on_mode_changed] {self.name}: ❌ ERREUR - Aucun capteur disponible")
                        QMessageBox.warning(self, "Erreur - Mode PID", 
                                          "Aucun capteur de température n'est disponible.\n\n"
                                          "Vérifiez que les capteurs DS18B20 sont connectés et fonctionnels.\n\n"
                                          "Cliquez sur l'icône ⚙ pour configurer les capteurs une fois qu'ils seront détectés.")
                        self.mode_group.button(0).setChecked(True)
                        return
                
                print(f"[DEBUG on_mode_changed] {self.name}: ✓ Capteur OK ('{temp_sensor}'), démarrage de l'asservissement...")
                try:
                    self.AstraDrew.startAserv()
                    self.buttonAsservOn = True
                    self.buttonRoseeConsigneOn = False
                    print(f"[DEBUG on_mode_changed] {self.name}: ✓ Asservissement démarré avec succès")
                except Exception as e:
                    print(f"[DEBUG on_mode_changed] {self.name}: ❌ EXCEPTION lors du démarrage PID: {type(e).__name__}: {str(e)}")
                    import traceback
                    traceback.print_exc()
                    QMessageBox.warning(self, "Erreur - Démarrage PID", 
                                      f"Impossible de démarrer le contrôle PID.\n\nErreur: {str(e)}")
                    self.mode_group.button(0).setChecked(True)
                    return
            elif mid == 2: # Auto
                print(f"[DEBUG on_mode_changed] {self.name}: Mode AUTO activé - vérification du BME280...")
                self.p_row_widget.setVisible(True) # On garde la puissance visible pour voir ce que fait le PID
                self.c_row_widget.setVisible(True) # On garde la consigne visible pour voir la cible calculée
                bme_temp = self.AstraDrew.get_bmeTemp()
                print(f"[DEBUG on_mode_changed] {self.name}: bme_temp={bme_temp}, TEMPUNAVAIL={self.AstraDrew.TEMPUNAVAIL}")
                print(f"[DEBUG on_mode_changed] {self.name}: bme_temp == TEMPUNAVAIL: {bme_temp == self.AstraDrew.TEMPUNAVAIL}")
                if bme_temp == self.AstraDrew.TEMPUNAVAIL:
                    print(f"[DEBUG on_mode_changed] {self.name}: ❌ ERREUR - BME280 non disponible, affichage du message")
                    QMessageBox.warning(self, "Erreur - Mode Auto", 
                                      "Le capteur BME280 est requis pour le mode automatique (asservissement à la température de rosée).\n\n"
                                      "Vérifiez que le capteur BME280 est connecté et fonctionnel.")
                    self.mode_group.button(0).setChecked(True)
                    return
                print(f"[DEBUG on_mode_changed] {self.name}: ✓ BME280 OK, démarrage du mode auto...")
                try:
                    self.AstraDrew.startAserv()
                    self.AstraDrew.set_asservTempRosee()
                    self.buttonAsservOn = True
                    self.buttonRoseeConsigneOn = True
                    print(f"[DEBUG on_mode_changed] {self.name}: ✓ Mode auto démarré avec succès")
                except Exception as e:
                    print(f"[DEBUG on_mode_changed] {self.name}: ❌ EXCEPTION lors du démarrage mode auto: {type(e).__name__}: {str(e)}")
                    import traceback
                    traceback.print_exc()
                    QMessageBox.warning(self, "Erreur - Démarrage Mode Auto", 
                                      f"Impossible de démarrer le mode automatique.\n\nErreur: {str(e)}")
                    self.mode_group.button(0).setChecked(True)
                    return
        except Exception as e:
            print(f"[DEBUG on_mode_changed] {self.name}: ❌❌ EXCEPTION GLOBALE: {type(e).__name__}: {str(e)}")
            import traceback
            traceback.print_exc()
            QMessageBox.critical(self, "Erreur - Changement de mode", 
                               f"Une erreur inattendue s'est produite lors du changement de mode.\n\n"
                               f"Erreur: {str(e)}\n\n"
                               f"Le mode a été réinitialisé en mode manuel.")
            self.mode_group.button(0).setChecked(True)

    def set_power(self, v): self.p_val.setText(f"{v}%"); self.AstraDrew.set_ratio(v)
    def set_consigne(self, v): self.c_val.setText(f"{v/10:.1f}°C"); self.AstraDrew.set_cmdTemp(v/10)
    def open_config_dialog(self): SensorConfigDialog(self, [self]).exec_()

    def update_text_fields(self):
        if self.buttonAsservOn:
            r = self.AstraDrew.get_ratio(); self.p_slider.setValue(int(r)); self.p_val.setText(f"{int(r)}%")
        
        cmd = None
        if self.buttonRoseeConsigneOn:
            # Mettre à jour la consigne depuis le point de rosée avant de l'afficher
            self.AstraDrew.updateCmdTempfromTempRosee()
            cmd = self.AstraDrew.get_cmdTemp()
            # Log pour debug : vérifier la valeur reçue et la condition
            if cmd > -50:
                self.c_slider.setValue(int(cmd*10))
                self.c_val.setText(f"{cmd:.1f}°C")
            else:
                print(f"[DEBUG HMI update_text_fields] {self.name}: cmd={cmd:.1f}°C rejeté (condition cmd > -50 non remplie)")
        t = self.AstraDrew.get_temp()
        self.lbl_temp.setText(f"{t:.1f}°" if t != self.AstraDrew.TEMPUNAVAIL else "NC")
        
        # Récupérer les valeurs pour affichage et logging
        amb_temp = self.AstraDrew.get_bmeTemp()
        amb_hum = self.AstraDrew.get_bmeHumidity()
        dew = self.AstraDrew.get_bmeTempRosee()
        
        self.lbl_amb.setText(f"{amb_temp:.1f}°")
        self.lbl_hum.setText(f"{amb_hum:.0f}%")
        self.lbl_dew.setText(f"{dew:.1f}°" if dew != self.AstraDrew.ROSEEUNAVAIL else "--")
        
        # Log des valeurs affichées dans l'interface (si mode auto)
        if self.buttonRoseeConsigneOn and cmd is not None:
            print(f"[HMI] {self.name}: "
                  f"Affichage - Temp_ambiante={amb_temp:.1f}°C | "
                  f"Humidité={amb_hum:.0f}% | "
                  f"Point_rosée={dew:.1f}°C | "
                  f"Consigne={cmd:.1f}°C")


class MainPwmWindow(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.initUI()

    def initUI(self):
        self.setStyleSheet("background-color: #0f172a;")
        main_vbox = QVBoxLayout(self)
        main_vbox.setContentsMargins(0, 0, 0, 0)
        main_vbox.setSpacing(0)

        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.NoFrame)
        scroll.setStyleSheet("QScrollArea { background-color: #0f172a; border: none; }")
        
        container = QWidget()
        container.setStyleSheet("background-color: #0f172a;")
        self.list_layout = QVBoxLayout(container)
        self.list_layout.setContentsMargins(20, 20, 20, 20)
        self.list_layout.setSpacing(15)

        self.widgets = []
        for name in ["AstraPwm1", "AstraPwm2"]:
            w = DrewControl(name, self)
            self.widgets.append(w)
            self.list_layout.addWidget(w)
        self.list_layout.addStretch()
        
        scroll.setWidget(container)
        main_vbox.addWidget(scroll)

        # Footer Status
        footer = QFrame()
        footer.setFixedHeight(60)
        footer.setStyleSheet("background-color: #1e293b; border-top: 1px solid #334155;")
        foot_lay = QHBoxLayout(footer)
        foot_lay.setContentsMargins(25, 0, 25, 0)
        
        self.bme_led = QLabel("⬤")
        self.bme_type = QLabel("Environnement: --")
        self.bme_type.setStyleSheet("color: #94a3b8; font-weight: 700; font-size: 11px; text-transform: uppercase;")
        
        cfg_pid = QPushButton("PARAMÈTRES PID")
        cfg_pid.setCursor(Qt.PointingHandCursor)
        cfg_pid.setStyleSheet("""
            QPushButton { background-color: #334155; color: #f8fafc; padding: 8px 20px; border-radius: 6px; font-weight: 800; font-size: 11px; }
            QPushButton:hover { background-color: #475569; }
        """)
        cfg_pid.clicked.connect(self.open_pid_config)
        
        foot_lay.addWidget(self.bme_led); foot_lay.addWidget(self.bme_type)
        foot_lay.addStretch(); foot_lay.addWidget(cfg_pid)
        main_vbox.addWidget(footer)

        self.timer = QTimer()
        self.timer.timeout.connect(self.update_all)
        self.timer.start(1000)

    def update_all(self):
        for w in self.widgets: w.update_text_fields()
        if self.widgets:
            amb = self.widgets[0].AstraDrew.get_bmeTemp()
            is_ok = amb != self.widgets[0].AstraDrew.TEMPUNAVAIL
            self.bme_led.setStyleSheet(f"color: {'#10b981' if is_ok else '#ef4444'}; font-size: 14px;")
            self.bme_type.setText(f"Environnement: {'BME280' if is_ok else 'DÉCONNECTÉ'}")

    def open_pid_config(self): PIDConfigDialogShared(self, self.widgets).exec_()

    def closeEvent(self, event):
        for w in self.widgets: w.AstraDrew.end()
        event.accept()

if __name__ == '__main__':
    app = QApplication(sys.argv)
    main = MainPwmWindow()
    main.showMaximized()
    sys.exit(app.exec_())
