#!/usr/bin/env bash
set -euo pipefail

SCRIPT_NAME="$(basename "$0")"

SCOPE="standard"
REMOVE_INDI=1
DRY_RUN=0
ASSUME_YES=0

REAL_USER="${SUDO_USER:-${USER:-root}}"
REAL_HOME="${HOME:-/root}"

BOOT_CONFIG_FILE="/boot/firmware/config.txt"
CHRONY_ASTRALIM_CONF="/etc/chrony/conf.d/chronyastralim.conf"

DESKTOP_FILES=(
    "AstraDIY.desktop"
    "AstraGPS.desktop"
    "Mise à Jour Astralim.desktop"
    "Install_INDIAstrAlim.desktop"
)

INDI_BINS=(
    "/usr/bin/indi_astralim_focuser"
    "/usr/bin/indi_astralim_relays"
    "/usr/bin/indi_astralim_system"
    "/usr/bin/indi_astralim_heater"
    "/usr/local/bin/indi_astralim_focuser"
    "/usr/local/bin/indi_astralim_relays"
    "/usr/local/bin/indi_astralim_system"
    "/usr/local/bin/indi_astralim_heater"
)

BOOT_BLOCK_MARKERS=(
    "# Begin AstrAlim GPS|# End AstrAlim GPS"
    "# Begin AstrAlim Set I2C Baudrate|# End AstrAlim Set I2C Baudrate"
    "# Begin AstrAlim 1 Wire|# End AstrAlim 1 wire"
    "# Begin AstrAlim PWM Outputs|# End AstrAlim PWM Outputs"
    "# Begin AstrAlimPowerManagement  Outputs|# End AstrAlimPowerManagement Outputs"
)

AGGRESSIVE_APT_PACKAGES=(
    "python3"
    "python3-libgpiod"
    "python3-smbus"
    "python3-pyqt5"
    "gpiod"
    "i2c-tools"
    "python3-ntplib"
    "python3-gps"
    "gpsd"
    "chrony"
    "pps-tools"
    "build-essential"
    "cmake"
    "libindi-dev"
    "libgpiod-dev"
    "pkg-config"
)

usage() {
    cat <<EOF
Usage: ${SCRIPT_NAME} [options]

Options:
  --scope <standard|minimal|aggressive>   Uninstall scope (default: standard)
  --remove-indi                           Remove INDI AstrAlim drivers (default)
  --keep-indi                             Keep INDI AstrAlim drivers
  --dry-run                               Print actions without modifying the system
  --yes                                   Skip the main confirmation prompt
  --help                                  Show this help message
EOF
}

log_ok() {
    printf '[OK] %s\n' "$*"
}

log_skip() {
    printf '[SKIP] %s\n' "$*"
}

log_warn() {
    printf '[WARN] %s\n' "$*" >&2
}

log_err() {
    printf '[ERR] %s\n' "$*" >&2
}

fail() {
    log_err "$*"
    exit 1
}

parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --scope)
                shift
                [[ $# -gt 0 ]] || fail "--scope requires a value"
                SCOPE="$1"
                ;;
            --scope=*)
                SCOPE="${1#*=}"
                ;;
            --remove-indi)
                REMOVE_INDI=1
                ;;
            --keep-indi)
                REMOVE_INDI=0
                ;;
            --dry-run)
                DRY_RUN=1
                ;;
            --yes)
                ASSUME_YES=1
                ;;
            --help|-h)
                usage
                exit 0
                ;;
            *)
                fail "Unknown option: $1"
                ;;
        esac
        shift
    done

    case "$SCOPE" in
        standard|minimal|aggressive) ;;
        *) fail "Invalid scope '$SCOPE' (expected: standard|minimal|aggressive)" ;;
    esac
}

require_root() {
    if [[ "${EUID}" -ne 0 ]]; then
        fail "This script must be run as root (use sudo)."
    fi
}

detect_real_user() {
    local detected_home=""

    if command -v getent >/dev/null 2>&1; then
        detected_home="$(getent passwd "$REAL_USER" 2>/dev/null | cut -d: -f6 || true)"
    fi

    if [[ -n "$detected_home" ]]; then
        REAL_HOME="$detected_home"
    fi
}

confirm_or_exit() {
    local prompt="$1"
    local expected="${2:-yes}"
    local force_prompt="${3:-0}"
    local answer=""

    if [[ "$DRY_RUN" -eq 1 ]]; then
        log_ok "[DRY-RUN] Confirmation skipped: ${prompt}"
        return 0
    fi

    if [[ "$force_prompt" -eq 0 && "$ASSUME_YES" -eq 1 ]]; then
        log_ok "Confirmation skipped due to --yes"
        return 0
    fi

    if [[ ! -t 0 ]]; then
        fail "Interactive confirmation required but stdin is not a TTY."
    fi

    printf "%s" "$prompt"
    read -r answer
    if [[ "$answer" != "$expected" ]]; then
        fail "Confirmation denied."
    fi
}

remove_file_if_exists() {
    local file_path="$1"

    if [[ -e "$file_path" || -L "$file_path" ]]; then
        if [[ "$DRY_RUN" -eq 1 ]]; then
            log_ok "[DRY-RUN] Remove file: $file_path"
        else
            rm -f -- "$file_path"
            log_ok "Removed file: $file_path"
        fi
    else
        log_skip "File not present: $file_path"
    fi
}

remove_dir_if_exists() {
    local dir_path="$1"

    if [[ -d "$dir_path" ]]; then
        if [[ "$DRY_RUN" -eq 1 ]]; then
            log_ok "[DRY-RUN] Remove directory: $dir_path"
        else
            rm -rf -- "$dir_path"
            log_ok "Removed directory: $dir_path"
        fi
    else
        log_skip "Directory not present: $dir_path"
    fi
}

remove_glob_if_exists() {
    local target_dir="$1"
    local pattern="$2"
    local matches=()

    if [[ ! -d "$target_dir" ]]; then
        log_skip "Directory not present: $target_dir"
        return 0
    fi

    shopt -s nullglob
    matches=("$target_dir"/$pattern)
    shopt -u nullglob

    if [[ "${#matches[@]}" -eq 0 ]]; then
        log_skip "No match for pattern '$pattern' in $target_dir"
        return 0
    fi

    if [[ "$DRY_RUN" -eq 1 ]]; then
        log_ok "[DRY-RUN] Remove ${#matches[@]} file(s) matching '$pattern' in $target_dir"
    else
        rm -f -- "${matches[@]}"
        log_ok "Removed ${#matches[@]} file(s) matching '$pattern' in $target_dir"
    fi
}

backup_file_with_timestamp() {
    local file_path="$1"
    local backup_path=""

    if [[ ! -f "$file_path" ]]; then
        log_skip "Cannot backup missing file: $file_path"
        return 0
    fi

    backup_path="${file_path}.bak.$(date +%Y%m%d-%H%M%S)"
    if [[ "$DRY_RUN" -eq 1 ]]; then
        log_ok "[DRY-RUN] Backup file: $file_path -> $backup_path"
    else
        cp -a -- "$file_path" "$backup_path"
        log_ok "Backup created: $backup_path"
    fi
}

remove_block_by_markers() {
    local file_path="$1"
    local begin_marker="$2"
    local end_marker="$3"
    local tmp_file=""

    if [[ ! -f "$file_path" ]]; then
        log_skip "File not present: $file_path"
        return 0
    fi

    if ! grep -Fq "$begin_marker" "$file_path"; then
        log_skip "Block begin marker not found: '$begin_marker' in $file_path"
        return 0
    fi

    if ! grep -Fq "$end_marker" "$file_path"; then
        log_warn "End marker '$end_marker' not found in $file_path, removing until EOF from begin marker."
    fi

    if [[ "$DRY_RUN" -eq 1 ]]; then
        log_ok "[DRY-RUN] Remove block '$begin_marker' ... '$end_marker' in $file_path"
        return 0
    fi

    tmp_file="$(mktemp)"
    awk -v begin="$begin_marker" -v end="$end_marker" '
        BEGIN { in_block=0 }
        {
            if (index($0, begin) > 0) { in_block=1; next }
            if (in_block && index($0, end) > 0) { in_block=0; next }
            if (!in_block) print
        }
    ' "$file_path" > "$tmp_file"

    if cmp -s "$file_path" "$tmp_file"; then
        rm -f -- "$tmp_file"
        log_skip "No change applied while removing block '$begin_marker' in $file_path"
        return 0
    fi

    cat "$tmp_file" > "$file_path"
    rm -f -- "$tmp_file"
    log_ok "Removed block '$begin_marker' ... '$end_marker' in $file_path"
}

cleanup_base_files() {
    log_ok "Starting base AstraDIY file cleanup"

    remove_dir_if_exists "/opt/AstraDIY"
    remove_dir_if_exists "/opt/AstrAlim"

    remove_file_if_exists "/etc/profile.d/AstraDIY.sh"
    remove_file_if_exists "/etc/profile.d/AstrAlim.sh"

    remove_glob_if_exists "${REAL_HOME}/.config/autostart" "Astra*.desktop"
    if [[ "$REAL_HOME" != "/root" ]]; then
        remove_glob_if_exists "/root/.config/autostart" "Astra*.desktop"
    fi

    local desktop_dir=""
    local desktop_file=""
    for desktop_dir in "${REAL_HOME}/Desktop" "${REAL_HOME}/Bureau"; do
        for desktop_file in "${DESKTOP_FILES[@]}"; do
            remove_file_if_exists "${desktop_dir}/${desktop_file}"
        done
    done

    remove_file_if_exists "${REAL_HOME}/.local/share/icons/hicolor/96x96/apps/astralim.png"
}

cleanup_standard_scope() {
    log_ok "Applying standard scope cleanup"

    remove_file_if_exists "$CHRONY_ASTRALIM_CONF"

    if [[ ! -f "$BOOT_CONFIG_FILE" ]]; then
        log_skip "Boot config not present: $BOOT_CONFIG_FILE"
        return 0
    fi

    local marker_pair=""
    local begin_marker=""
    local has_boot_block=0
    for marker_pair in "${BOOT_BLOCK_MARKERS[@]}"; do
        begin_marker="${marker_pair%%|*}"
        if grep -Fq "$begin_marker" "$BOOT_CONFIG_FILE"; then
            has_boot_block=1
            break
        fi
    done

    if [[ "$has_boot_block" -eq 0 ]]; then
        log_skip "No AstraDIY block marker found in $BOOT_CONFIG_FILE"
        return 0
    fi

    backup_file_with_timestamp "$BOOT_CONFIG_FILE"

    local end_marker=""
    for marker_pair in "${BOOT_BLOCK_MARKERS[@]}"; do
        begin_marker="${marker_pair%%|*}"
        end_marker="${marker_pair##*|}"
        remove_block_by_markers "$BOOT_CONFIG_FILE" "$begin_marker" "$end_marker"
    done
}

cleanup_indi_if_requested() {
    if [[ "$REMOVE_INDI" -ne 1 ]]; then
        log_skip "INDI removal disabled by --keep-indi"
        return 0
    fi

    log_ok "Removing INDI AstrAlim artifacts"

    local indi_bin=""
    for indi_bin in "${INDI_BINS[@]}"; do
        remove_file_if_exists "$indi_bin"
    done

    local indi_xml_candidates=()
    local indi_datadir=""

    if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists indi 2>/dev/null; then
        indi_datadir="$(pkg-config --variable=datadir indi 2>/dev/null || true)"
        if [[ -n "$indi_datadir" ]]; then
            indi_xml_candidates+=("${indi_datadir}/indi/indi_astralim.xml")
        fi
    fi

    indi_xml_candidates+=(
        "/usr/share/indi/indi_astralim.xml"
        "/usr/local/share/indi/indi_astralim.xml"
    )

    local indi_xml=""
    for indi_xml in "${indi_xml_candidates[@]}"; do
        remove_file_if_exists "$indi_xml"
    done

    if [[ "$DRY_RUN" -eq 1 ]]; then
        log_ok "[DRY-RUN] Restart INDI service (indiserver or indi-web)"
        return 0
    fi

    if systemctl restart indiserver 2>/dev/null; then
        log_ok "Restarted service: indiserver"
    elif systemctl restart indi-web 2>/dev/null; then
        log_ok "Restarted service: indi-web"
    else
        log_skip "No INDI service restarted (indiserver/indi-web unavailable)"
    fi
}

purge_aggressive_packages() {
    log_warn "Aggressive mode selected: apt purge may remove packages used by other software."
    confirm_or_exit "Type PURGE to continue apt purge: " "PURGE" "1"

    if [[ "$DRY_RUN" -eq 1 ]]; then
        log_ok "[DRY-RUN] apt-get purge -y ${AGGRESSIVE_APT_PACKAGES[*]}"
        log_ok "[DRY-RUN] apt-get autoremove -y"
        return 0
    fi

    apt-get purge -y "${AGGRESSIVE_APT_PACKAGES[@]}"
    log_ok "APT purge completed"

    apt-get autoremove -y
    log_ok "APT autoremove completed"
}

main() {
    parse_args "$@"
    require_root
    detect_real_user

    log_ok "Real user detected: ${REAL_USER} (${REAL_HOME})"
    log_ok "Options: scope=${SCOPE}, remove_indi=${REMOVE_INDI}, dry_run=${DRY_RUN}, assume_yes=${ASSUME_YES}"

    confirm_or_exit "Proceed with AstraDIY uninstall? Type yes to continue: " "yes" "0"

    cleanup_base_files

    case "$SCOPE" in
        minimal)
            log_ok "Minimal scope selected: keeping GPS/boot AstraDIY configuration unchanged"
            ;;
        standard|aggressive)
            cleanup_standard_scope
            ;;
    esac

    cleanup_indi_if_requested

    if [[ "$SCOPE" == "aggressive" ]]; then
        purge_aggressive_packages
    fi

    log_ok "Uninstall complete."
    log_warn "A reboot may be required for all changes to take effect."
}

main "$@"
