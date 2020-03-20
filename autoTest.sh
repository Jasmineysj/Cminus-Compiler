cd build
export PATH="$(pwd):$PATH"
export LIBRARY_PATH="$(pwd):$LIBRARY_PATH"

cd ../ta_testcases
dir=$(ls)
echo $dir


for i in $dir
do 
    cd $i
    echo "=====================" $i " =============================="
    path=$(pwd)
    for testcase in $(ls $path | grep .cminus):
    do 
        echo $testcase
        cminusc ${testcase%:}
        ./${testcase%.*}
    done 
    cd ..
done






