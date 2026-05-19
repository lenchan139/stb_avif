#!/bin/bash
# Monitor MAE values until 10-bit decoding improves
# Usage: ./monitor_mae.sh [target_mae_threshold]

THRESHOLD=${1:-100}  # Default target MAE threshold for 10-bit Y plane

echo "=========================================="
echo "MAE Monitor - Target 10-bit Y MAE < $THRESHOLD"
echo "=========================================="
echo ""

count=0
while true; do
    count=$((count + 1))
    echo "--- Check #$count ---"
    
    # Build test decoder
    cc -O2 -o test_decode_run tests/test_decode.c -lm 2>/dev/null || {
        echo "ERROR: Failed to compile test decoder"
        exit 1
    }
    
    # Run MAE calculation
    MAE_OUTPUT=$(python3 tools/mae_calc.py 2>&1)
    
    # Extract 10-bit Y MAE
    FOX_10BPC_Y_MAE=$(echo "$MAE_OUTPUT" | grep "fox.profile0.10bpc.yuv420.avif.*Y" | awk '{print $3}')
    
    # Extract 8-bit Y MAE for comparison
    FOX_8BPC_Y_MAE=$(echo "$MAE_OUTPUT" | grep "fox.profile0.8bpc.yuv420.avif.*Y" | awk '{print $3}')
    
    echo "Current MAE:"
    echo "  8-bit fox Y:  $FOX_8BPC_Y_MAE"
    echo "  10-bit fox Y: $FOX_10BPC_Y_MAE"
    echo ""
    
    # Check if 10-bit MAE is below threshold
    if (( $(echo "$FOX_10BPC_Y_MAE < $THRESHOLD" | bc -l) )); then
        echo "✓ SUCCESS! 10-bit Y MAE ($FOX_10BPC_Y_MAE) is below threshold ($THRESHOLD)"
        echo "  Issue appears to be FIXED!"
        break
    else
        echo "✗ 10-bit Y MAE ($FOX_10BPC_Y_MAE) still above threshold ($THRESHOLD)"
        echo "  Issue NOT yet fixed. Retrying in 2 seconds..."
        echo ""
        sleep 2
    fi
done

# Cleanup
rm -f test_decode_run

echo ""
echo "=========================================="
echo "Monitoring complete after $count checks"
echo "=========================================="
