#!/usr/bin/env python3
import subprocess

def get_chronyc_tracking_data():
    try:
        # Exécute la commande chronyc tracking et récupère la sortie
        output = subprocess.check_output(['chronyc', 'tracking'], text=True)

        # Analyse la sortie pour extraire les valeurs d'offset et de RMS offset
        lines = output.split('\n')
        offset_line = next(line for line in lines if 'Last offset' in line)
        rms_offset_line = next(line for line in lines if 'RMS offset' in line)

        offset_parts = offset_line.split(':')[1].strip().split()
        rms_offset_parts = rms_offset_line.split(':')[1].strip().split()

        offset_value = float(offset_parts[0])
        offset_unit = offset_parts[1]

        rms_offset_value = float(rms_offset_parts[0])
        rms_offset_unit = rms_offset_parts[1]

        return offset_value, offset_unit, rms_offset_value, rms_offset_unit

    except subprocess.CalledProcessError as e:
        print(f"Erreur lors de l'exécution de la commande chronyc tracking : {e}")
        return None, None, None, None

if __name__ == "__main__":
    offset_value, offset_unit, rms_offset_value, rms_offset_unit = get_chronyc_tracking_data()


    if offset_value is not None and rms_offset_value is not None:
        if  "sec" in offset_unit: 
            offus=offset_value*1000*1000
            print(f"Offset     : {offus:+.3f} us")
        else:
           print(f"Offset : {offset_value} {offset_unit}")
        if  "sec" in rms_offset_unit:
            rms_us=rms_offset_value*1000*1000
            print(f"RMS Offset : {rms_us:+.3f} us")
        else:
            print(f"RMS Offset : {rms_offset_value} {rms_offset_unit}")
    else:
        print("Impossible de récupérer les données de chronyc tracking.")

