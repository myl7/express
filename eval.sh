# Config
x_bitlen=11
jobs=8
log=$(pwd)/express.log

n=$(python -c "x = 2 ** ${x_bitlen}; print(x)")
export LD_LIBRARY_PATH="$(pwd)/../openssl-1.1.1w/usr/lib:$LD_LIBRARY_PATH"

(cd serverA && ./serverA 127.0.0.1:4442 1 $jobs $n 1024 >> $log &)
(cd serverB && ./serverB 1 $jobs $n 1024 &)
sleep 1
./client/client 127.0.0.1:4443 127.0.0.1:4442 1 1024 &> /dev/null
pkill serverB
pkill serverA
sleep 1
