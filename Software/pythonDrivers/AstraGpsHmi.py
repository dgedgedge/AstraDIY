#!/usr/bin/env python3
import sys
import time
from PyQt5.QtCore import QTimer, Qt
from PyQt5.QtWidgets import QApplication, QWidget, QVBoxLayout, QHBoxLayout, QLabel, QFrame, QScrollArea
from AstraGps import AstraGps

QApplication.setApplicationName("AstraGPS")

class MainGpsWindow(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.gps = AstraGps().get_instance()
        self.initUI()

    def initUI(self):
        # Style global
        self.setStyleSheet("""
            MainGpsWindow { 
                background-color: #0f172a; 
            }
            QScrollArea {
                border: none;
                background-color: #0f172a;
            }
            #container {
                background-color: #0f172a;
            }
            .card { 
                background-color: #1e293b; 
                border-radius: 12px; 
                border: 1px solid #334155;
            }
            .title { 
                color: #64748b; 
                font-size: 10px; 
                font-weight: 800; 
                text-transform: uppercase; 
                letter-spacing: 1.5px; 
            }
            .label-small { 
                color: #94a3b8; 
                font-size: 9px; 
                font-weight: 700; 
                text-transform: uppercase; 
            }
            .value-large { 
                color: #f8fafc; 
                font-size: 16px; 
                font-weight: 800; 
                font-family: 'JetBrains Mono', 'Monospace'; 
            }
            .value-clock { 
                color: #fbbf24; 
                font-size: 32px; 
                font-weight: 800; 
                font-family: 'JetBrains Mono', 'Monospace'; 
            }
        """)
        
        main_layout = QVBoxLayout(self)
        main_layout.setContentsMargins(0, 0, 0, 0)
        
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        container = QWidget()
        container.setObjectName("container")
        content_layout = QVBoxLayout(container)
        content_layout.setContentsMargins(20, 20, 20, 20)
        content_layout.setSpacing(20)

        # --- SECTION GPS ---
        gps_card = QFrame()
        gps_card.setProperty("class", "card")
        gps_lay = QVBoxLayout(gps_card)
        gps_lay.setContentsMargins(20, 15, 20, 15)
        gps_lay.setSpacing(15)
        
        # Header GPS
        gps_header = QHBoxLayout()
        gps_title = QLabel("🛰️ SIGNAL GPS")
        gps_title.setProperty("class", "title")
        self.gps_module_status = QLabel("MODULE: --")
        self.gps_module_status.setStyleSheet("color: #94a3b8; font-weight: 700; font-size: 10px;")
        self.gps_sat_count = QLabel("SAT: --")
        self.gps_sat_count.setStyleSheet("color: #94a3b8; font-weight: 700; font-size: 10px;")
        self.gps_led = QLabel("⬤")
        self.gps_fix = QLabel("NO FIX")
        self.gps_fix.setStyleSheet("color: #ef4444; font-weight: 900; font-size: 12px;")
        gps_header.addWidget(gps_title); gps_header.addWidget(self.gps_module_status); gps_header.addWidget(self.gps_sat_count); gps_header.addStretch(); gps_header.addWidget(self.gps_led); gps_header.addWidget(self.gps_fix)
        gps_lay.addLayout(gps_header)
        
        # Ligne Coordonnées
        coords_row = QHBoxLayout()
        coords_row.setSpacing(30)
        
        def add_data_block(parent_lay, label, color):
            v = QVBoxLayout(); v.setSpacing(2)
            l = QLabel(label); l.setProperty("class", "label-small")
            val = QLabel("--"); val.setProperty("class", "value-large"); val.setStyleSheet(f"color: {color};")
            v.addWidget(l); v.addWidget(val); parent_lay.addLayout(v); return val

        self.val_lat = add_data_block(coords_row, "LATITUDE", "#38bdf8")
        self.val_lon = add_data_block(coords_row, "LONGITUDE", "#38bdf8")
        self.val_alt = add_data_block(coords_row, "ALTITUDE (M)", "#10b981")
        coords_row.addStretch()
        gps_lay.addLayout(coords_row)
        
        # Ligne Time & Sat
        time_row = QHBoxLayout()
        self.val_gps_time = add_data_block(time_row, "HEURE GPS (UTC)", "#fbbf24")
        time_row.addSpacing(40)
        self.val_pps = add_data_block(time_row, "SIGNAL PPS", "#94a3b8")
        time_row.addStretch()
        gps_lay.addLayout(time_row)
        
        content_layout.addWidget(gps_card)

        # --- SECTION NTP ---
        ntp_card = QFrame()
        ntp_card.setProperty("class", "card")
        ntp_lay = QVBoxLayout(ntp_card)
        ntp_lay.setContentsMargins(20, 15, 20, 15)
        ntp_lay.setSpacing(15)
        
        # Header NTP
        ntp_header = QHBoxLayout()
        ntp_title = QLabel("🕒 SYNCHRONISATION TEMPS (NTP)")
        ntp_title.setProperty("class", "title")
        self.ntp_led = QLabel("⬤")
        ntp_header.addWidget(ntp_title); ntp_header.addStretch(); ntp_header.addWidget(self.ntp_led)
        ntp_lay.addLayout(ntp_header)
        
        # Horloge & Stats
        clock_row = QHBoxLayout()
        self.val_clock = QLabel("00:00:00")
        self.val_clock.setProperty("class", "value-clock")
        
        ntp_stats = QVBoxLayout()
        ntp_stats.setSpacing(2)
        self.val_prec = QLabel("PRÉCISION: -- µs"); self.val_prec.setStyleSheet("color: #64748b; font-size: 10px; font-weight: 700;")
        self.val_off = QLabel("DÉCALAGE: -- µs"); self.val_off.setStyleSheet("color: #64748b; font-size: 10px; font-weight: 700;")
        ntp_stats.addStretch(); ntp_stats.addWidget(self.val_prec); ntp_stats.addWidget(self.val_off); ntp_stats.addStretch()
        
        clock_row.addWidget(self.val_clock); clock_row.addStretch(); clock_row.addLayout(ntp_stats)
        ntp_lay.addLayout(clock_row)
        
        content_layout.addWidget(ntp_card)
        content_layout.addStretch()
        
        scroll.setWidget(container)
        main_layout.addWidget(scroll)

        # Timer
        self.timer = QTimer()
        self.timer.timeout.connect(self.update_fields)
        self.timer.start(1000)

    def update_fields(self):
        # GPS - Présence du module
        gps_present = self.gps.gpsIsPresent()
        self.gps_module_status.setText(f"MODULE: {'✓ DÉTECTÉ' if gps_present else '✗ ABSENT'}")
        self.gps_module_status.setStyleSheet(f"color: {'#10b981' if gps_present else '#ef4444'}; font-weight: 700; font-size: 10px;")
        
        # GPS - Nombre de satellites
        sat_count = self.gps.gpsSatVisible()
        self.gps_sat_count.setText(f"SAT: {sat_count}")
        sat_color = "#10b981" if sat_count >= 4 else "#f59e0b" if sat_count > 0 else "#94a3b8"
        self.gps_sat_count.setStyleSheet(f"color: {sat_color}; font-weight: 700; font-size: 10px;")
        
        # GPS - Fix
        sync = self.gps.gpsSyncState()
        fix_ok = str(sync) in ["2", "3", "2D", "3D"]
        self.gps_fix.setText(f"FIX {sync}D" if fix_ok else "NO FIX")
        self.gps_fix.setStyleSheet(f"color: {'#10b981' if fix_ok else '#ef4444'}; font-weight: 900; font-size: 12px;")
        self.gps_led.setStyleSheet(f"color: {'#10b981' if fix_ok else '#ef4444'}; font-size: 14px;")
        
        lat, lon, alt = self.gps.gpsGetStrPosition()
        self.val_lat.setText(lat); self.val_lon.setText(lon); self.val_alt.setText(alt)
        self.val_gps_time.setText(str(self.gps.gpsTimeStamp()))
        self.val_pps.setText(f"COUNT: {self.gps.gpsCountPPS()}")
        
        # NTP
        ntp_ts = self.gps.ntpTimeStampS()
        self.val_clock.setText(time.strftime("%H:%M:%S", time.localtime(ntp_ts)))
        prec = self.gps.ntpTimePrecisionUs()
        self.val_prec.setText(f"PRÉCISION: {prec:.1f} µs")
        self.val_off.setText(f"DÉCALAGE: {self.gps.ntpTimeOffsetUs():.1f} µs")
        
        ntp_sync_ok = prec < 100000
        self.ntp_led.setStyleSheet(f"color: {'#10b981' if ntp_sync_ok else '#f59e0b'}; font-size: 14px;")

    def closeEvent(self, event):
        self.gps.stop()
        event.accept()

if __name__ == '__main__':
    app = QApplication(sys.argv)
    main = MainGpsWindow()
    main.show()
    sys.exit(app.exec_())
