set -euo pipefail

PORT=6000

make >/dev/null

./server $PORT &
SRV_PID=$!
sleep 0.5

cleanup() { kill $SRV_PID 2>/dev/null || true; }
trap cleanup EXIT

expect=$(cat <<'EOF'
OK 12
OK -2
OK -10.5
ERR EZDV divisao_por_zero
OK 12
OK 10
OK 7.3
ERR EINV entrada_invalida
ERR EINV entrada_invalida
ERR EZDV divisao_por_zero
EOF
)

got=$( (printf \
"ADD 10 2\n\
SUB 7 9\n\
MUL -3 3.5\n\
DIV 5 0\n\
10 + 2\n\
-5 * -2\n\
10.5 - 3.2\n\
SOMA 1 2\n\
123abc + 5\n\
8 / 0\n\
QUIT\n" | ./client 127.0.0.1 $PORT) | tr -d '\r' )

echo "$got" | grep -E '^(OK|ERR) ' > /tmp/got.$$ || true

if diff -u <(echo "$expect") /tmp/got.$$ ; then
  echo "[tests] OK"
else
  echo "[tests] FAIL"
  exit 1
fi
