#!/bin/bash
# Monitor AV1 decoder implementation progress
# Prompts to "compare av1 decoder to finish this c89 impl" until done

echo "=========================================="
echo "AV1 C89 Decoder Implementation Monitor"
echo "=========================================="
echo ""

TARGET_MAE=${1:-100}
check_count=0

while true; do
    check_count=$((check_count + 1))
    
    echo "=========================================="
    echo "Check #$check_count - $(date '+%H:%M:%S')"
    echo "=========================================="
    echo ""
    
    # Build and test
    echo "[Building test decoder...]"
    if ! cc -O2 -o test_decode_run tests/test_decode.c -lm 2>/dev/null; then
        echo "ERROR: Build failed!"
    fi
    
    # Get MAE values
    echo "[Running MAE check...]"
    MAE_OUTPUT=$(python3 tools/mae_calc.py 2>&1)
    
    FOX_10BPC_Y_MAE=$(echo "$MAE_OUTPUT" | grep "fox.profile0.10bpc.yuv420.avif.*Y" | awk '{print $3}')
    FOX_8BPC_Y_MAE=$(echo "$MAE_OUTPUT" | grep "fox.profile0.8bpc.yuv420.avif.*Y" | awk '{print $3}')
    
    echo "Current MAE:"
    echo "  8-bit fox Y:  $FOX_8BPC_Y_MAE"
    echo "  10-bit fox Y: $FOX_10BPC_Y_MAE"
    echo ""
    
    # Check if done
    is_done=0
    if command -v bc &> /dev/null; then
        if (( $(echo "$FOX_10BPC_Y_MAE < $TARGET_MAE" | bc -l) )); then
            is_done=1
        fi
    fi
    
    if [ "$is_done" -eq 1 ]; then
        echo "=========================================="
        echo "✓ TASK COMPLETE!"
        echo "  10-bit Y MAE ($FOX_10BPC_Y_MAE) < threshold ($TARGET_MAE)"
        echo "=========================================="
        rm -f test_decode_run
        break
    fi
    
    # Show prompt and wait for user
    echo "=========================================="
    echo "PROMPT: compare av1 decoder to finish this c89 impl"
    echo "=========================================="
    echo ""
    echo "Press [Enter] when ready to check again,"
    echo "or type a command to run (e.g., 'make', 'test', 'exit'):"
    read -r user_input
    
    # Handle user input
    if [ "$user_input" = "exit" ] || [ "$user_input" = "quit" ]; then
        echo "Exiting monitor."
        rm -f test_decode_run
        break
    elif [ -n "$user_input" ]; then
        echo "Running: $user_input"
        eval "$user_input"
        echo ""
    fi
done

echo ""
echo "Monitor finished after $check_count checks."
