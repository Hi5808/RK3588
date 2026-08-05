#!/bin/bash

VERSION="1.0.0"

print_header() {
    echo "╔══════════════════════════════════════════════════════════════╗"
    echo "║        RK3588 System Information Tool v$VERSION               ║"
    echo "╚══════════════════════════════════════════════════════════════╝"
    echo ""
}

print_section() {
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "$1"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
}

system_info() {
    print_section "System Information"

    echo "Hostname:        $(hostname)"
    echo "Kernel:          $(uname -r)"
    echo "OS:              $(cat /etc/os-release 2>/dev/null | grep PRETTY_NAME | cut -d'"' -f2)"
    echo "Architecture:    $(uname -m)"
    echo "Uptime:          $(uptime -p)"
    echo ""
}

cpu_info() {
    print_section "CPU Information"

    echo "Processor:       $(grep 'Model name' /proc/cpuinfo | head -1 | cut -d: -f2 | xargs)"
    echo "Cores:           $(grep -c ^processor /proc/cpuinfo)"
    echo "Threads:         $(grep -c ^processor /proc/cpuinfo)"
    echo "Arch:            ARMv8-A (64-bit)"

    if [ -f "/proc/cpuinfo" ]; then
        echo "Features:        $(grep 'Features' /proc/cpuinfo | head -1 | cut -d: -f2 | xargs)"
    fi

    echo "CPU Freq (Current):"
    for i in {0..7}; do
        if [ -f "/sys/devices/system/cpu/cpu$i/cpufreq/scaling_cur_freq" ]; then
            freq=$(cat /sys/devices/system/cpu/cpu$i/cpufreq/scaling_cur_freq 2>/dev/null)
            if [ ! -z "$freq" ]; then
                echo "  CPU$i:           $((freq / 1000)) MHz"
            fi
        fi
    done
    echo ""
}

gpu_info() {
    print_section "GPU Information"

    echo "GPU Model:       Mali-G610"
    echo "Cores:           6"
    echo "Peak FP32:       ~2.16 TFLOPS"
    echo "Peak FP16:       ~4.32 TFLOPS"

    if [ -f "/sys/class/devfreq/ff9a0000.gpu/cur_freq" ]; then
        freq=$(cat /sys/class/devfreq/ff9a0000.gpu/cur_freq 2>/dev/null)
        echo "Current Freq:    $((freq / 1000000)) MHz"
    fi

    if [ -f "/sys/class/devfreq/ff9a0000.gpu/max_freq" ]; then
        max_freq=$(cat /sys/class/devfreq/ff9a0000.gpu/max_freq 2>/dev/null)
        echo "Max Freq:        $((max_freq / 1000000)) MHz"
    fi
    echo ""
}

memory_info() {
    print_section "Memory Information"

    if [ -f "/proc/meminfo" ]; then
        total=$(grep MemTotal /proc/meminfo | awk '{print $2}')
        available=$(grep MemAvailable /proc/meminfo | awk '{print $2}')
        buffers=$(grep Buffers /proc/meminfo | awk '{print $2}')
        cached=$(grep Cached /proc/meminfo | awk '{print $2}')
        used=$((total - available))

        echo "Total:           $((total / 1024)) MB"
        echo "Used:            $((used / 1024)) MB"
        echo "Available:       $((available / 1024)) MB"
        echo "Buffers:         $((buffers / 1024)) MB"
        echo "Cached:          $((cached / 1024)) MB"
        echo "Usage:           $((used * 100 / total))%"
    fi
    echo ""
}

storage_info() {
    print_section "Storage Information"

    df -h | grep -E '^/dev/|Filesystem' | awk '{
        if (NR==1) {
            printf "%-20s %-10s %-10s %-10s %-10s\n", $1, "Size", "Used", "Avail", "Usage"
        } else {
            printf "%-20s %-10s %-10s %-10s %-10s\n", $1, $2, $3, $4, $5
        }
    }'
    echo ""
}

thermal_info() {
    print_section "Thermal Information"

    found=0
    for i in {0..10}; do
        if [ -f "/sys/class/thermal/thermal_zone$i/type" ]; then
            zone_name=$(cat /sys/class/thermal/thermal_zone$i/type)
            temp=$(cat /sys/class/thermal/thermal_zone$i/temp 2>/dev/null)

            if [ ! -z "$temp" ]; then
                temp_c=$(echo "scale=1; $temp / 1000" | bc)
                printf "%-25s %s°C\n" "$zone_name:" "$temp_c"
                found=1
            fi
        fi
    done

    if [ $found -eq 0 ]; then
        echo "No thermal zones found"
    fi
    echo ""
}

network_info() {
    print_section "Network Information"

    if [ -f "/etc/hostname" ]; then
        echo "Hostname:        $(cat /etc/hostname)"
    fi

    if command -v ip &> /dev/null; then
        echo "Interfaces:"
        ip link show | grep "^[0-9]" | awk '{print "  " $2}' | sed 's/:$//'
    fi
    echo ""
}

npu_info() {
    print_section "NPU Information"

    if [ -d "/proc/rknpu" ]; then
        echo "NPU Status:      Available"
        echo "Model:           RK3588 NPU"
        echo "Note:            See /toolkit/rknpu2/ for detailed NPU tools"
    else
        echo "NPU Status:      Not detected"
    fi
    echo ""
}

main() {
    print_header

    system_info
    cpu_info
    gpu_info
    memory_info
    storage_info
    thermal_info
    network_info
    npu_info

    echo "Generated: $(date '+%Y-%m-%d %H:%M:%S')"
    echo ""
}

main
