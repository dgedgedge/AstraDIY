#!/bin/bash
# Script pour corriger la configuration PWM dans /boot/firmware/config.txt

CONFIG_FILE="/boot/firmware/config.txt"
BACKUP_FILE="/boot/firmware/config.txt.backup.$(date +%Y%m%d_%H%M%S)"

echo "============================================================"
echo "  Correction de la configuration PWM"
echo "============================================================"

# Vérifier les permissions
if [ ! -w "$CONFIG_FILE" ]; then
    echo "❌ Erreur: Pas de permission d'écriture sur $CONFIG_FILE"
    echo "   Exécutez ce script avec sudo: sudo $0"
    exit 1
fi

# Créer une sauvegarde
echo "📋 Création d'une sauvegarde: $BACKUP_FILE"
cp "$CONFIG_FILE" "$BACKUP_FILE"

# Vérifier la configuration actuelle
echo ""
echo "Configuration actuelle:"
grep "pwm-2chan" "$CONFIG_FILE" || echo "  (non trouvée)"

# Corriger la configuration
echo ""
echo "🔧 Correction de la configuration..."
sed -i 's/dtoverlay=pwm-2chan,pin=18,func=2,pin2=13,func2=0/dtoverlay=pwm-2chan,pin=18,func=2,pin2=13,func2=4/g' "$CONFIG_FILE"

# Vérifier la nouvelle configuration
echo ""
echo "Nouvelle configuration:"
grep "pwm-2chan" "$CONFIG_FILE" || echo "  (non trouvée)"

echo ""
echo "============================================================"
echo "  ✅ Configuration corrigée!"
echo "============================================================"
echo ""
echo "⚠️  IMPORTANT: Un redémarrage est nécessaire pour que les"
echo "   changements prennent effet:"
echo "   sudo reboot"
echo ""
echo "📋 Sauvegarde créée: $BACKUP_FILE"
echo "============================================================"

