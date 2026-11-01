# bench.sh <engine> <outdir>
cd /tmp/claude-0/gtb; B=$1; O=$2; rm -rf $O; mkdir -p $O
ls loops/*.wav > all.txt; split -n l/2 -d all.txt bp_
for part in bp_00 bp_01; do ( while read f; do n=$(basename $f .wav); mkdir -p $O/$n; timeout 900 $B $f $O/$n > $O/$n/log.txt 2>&1; done < $part ) & done; wait
python3 -I score.py loops $O /tmp/claude-0/lib44
