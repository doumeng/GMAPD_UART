
###
 # @Author: doumeng 1159898567@qq.com
 # @Date: 2026-03-17 19:36:19
 # @LastEditors: doumeng 1159898567@qq.com
 # @LastEditTime: 2026-03-23 14:12:10
 # @FilePath: /GMAPD_UART/mbuild.sh
 # @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
### 
if [ ! -d "build" ]; then
    mkdir -p build
fi

cd ./build

rm -rf *

cmake ../ 

make -j8

# scp ./binOutput/K253154 root@192.168.20.100:/userdata/dm