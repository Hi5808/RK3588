#!/bin/bash

VERSION="1.0.0"
INTERVAL=1
MAX_TEMP=80000  # 80°C in millidegrees

print_header() {
    echo "RK3588 Thermal Monitor v$VERSION"
    echo "=================================="
    echo ""
}

get_temperature() {
    local zone=$1
    local path="/sys/class/thermal/thermal_zone$zone/temp"

    if [ -f "$path" ]; then
        cat "$path"
    else
        echo "N/A"
    fi
}

get_temp_formatted() {
    local temp=$1
    if [ "$temp" != "N/A" ]; then
        echo "scale=1; $temp / 1000" | bc
    else
        echo "N/A"
    fi
}

print_thermal_info() {
    echo "Thermal Zones:"
    for i in {0..10}; do
        path="/sys/class/thermal/thermal_zone$i/type"
        if [ -f "$path" ]; then
            zone_name=$(cat "$path")
            temp=$(get_temperature "$i")
            temp_c=$(get_temp_formatted "$temp")

            if [ "$temp" != "N/A" ]; then
                printf "  Zone %d (%s):     %s°C" "$i" "$zone_name" "$temp_c"

                if [ "$temp" != "N/A" ] && [ "$temp" -gt "$MAX_TEMP" ]; then
                    echo " [WARNING]"
                else
                    echo ""
                fi
            fi
        fi
    done
    echo ""
}

get_cpu_freq() {
    if [ -f "/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq" ]; then
        local freq=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq)
        echo "scale=0; $freq / 1000" | bc
    else
        echo "N/A"
    fi
}

print_cpu_info() {
    echo "CPU Information:"
    local cores=$(grep -c ^processor /proc/cpuinfo)
    echo "  Cores:           $cores"

    local freq=$(get_cpu_freq)
    if [ "$freq" != "N/A" ]; then
        echo "  Current Freq:    ${freq}0 MHz"
    fi

    if [ -f "/proc/cpuinfo" ]; then
        local model=$(grep "Model name" /proc/cpuinfo | head -1 | cut -d: -f2 | xargs)
        if [ ! -z "$model" ]; then
            echo "  Model:           $model"
        fi
    fi
    echo ""
}

print_memory_info() {
    echo "Memory Information:"
    if [ -f "/proc/meminfo" ]; then
        local total=$(grep MemTotal /proc/meminfo | awk '{print $2}')
        local available=$(grep MemAvailable /proc/meminfo | awk '{print $2}')
        local used=$((total - available))

        echo "  Total:           $((total / 1024)) MB"
        echo "  Used:            $((used / 1024)) MB"
        echo "  Available:       $((available / 1024)) MB"
        echo "  Usage:           $((used * 100 / total))%"
    fi
    echo ""
}

print_gpu_info() {
    echo "GPU Information:"
    if lspci 2>/dev/null | grep -qi mali; then
        echo "  GPU:             Mali-G610"
        echo "  Cores:           6"
        echo "  Max Freq:        ~900 MHz (estimated)"
    elif [ -f "/sys/class/devfreq/ff9a0000.gpu/cur_freq" ]; then
        local freq=$(cat /sys/class/devfreq/ff9a0000.gpu/cur_freq)
        echo "  GPU Freq:        $((freq / 1000000)) MHz"
    else
        echo "  GPU:             Mali (unknown state)"
    fi
    echo ""
}

watch_thermal() {
    clear
    print_header

    while true; do
        print_thermal_info
        print_cpu_info
        print_memory_info
        print_gpu_info

        echo "Last updated: $(date '+%Y-%m-%d %H:%M:%S')"
        echo "Press Ctrl+C to exit, refreshing in ${INTERVAL}s..."

        sleep "$INTERVAL"
        clear
        print_header
    done
}

# Handle arguments
case "${1:-watch}" in
    watch)
        if [ ! -z "$2" ]; then
            INTERVAL=$2
        fi
        watch_thermal
        ;;
    snapshot)
        print_header
        print_thermal_info
        print_cpu_info
        print_memory_info
        print_gpu_info
        echo "Last updated: $(date '+%Y-%m-%d %H:%M:%S')"
        ;;
    help)
        echo "RK3588 Thermal Monitor v$VERSION"
        echo "Usage: thermal_monitor.sh [command] [args]"
        echo ""
        echo "Commands:"
        echo "  watch [interval]  - Continuously monitor (default interval: 1s)"
        echo "  snapshot          - Single thermal report"
        echo "  help              - Show this help"
        ;;
    *)
        echo "Unknown command: $1"
        echo "Use 'thermal_monitor.sh help' for usage"
        exit 1
        ;;
esac
