# Hand

北京大学图形学课程小作业代码。

> 注：Assimp是经过裁剪的版本，只支持FBX文件的导入

# 程序使用说明
在 MacOS 上写的作业。
```shell
# 0. Clone from github
git clone https://github.com/xuehaonan27/hand-graphics-homework-main.git

# 1. To root directory
cd hand-graphics-homework-main

# 2. Build the project
mkdir build
cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5
make
cd src/

# 3. Run the binary
./Hand
```

进入 GUI 后，默认处于 Completion Show 模式。
1. 按数字按键 1 展示 手势1：抓握
2. 按数字按键 2 展示 手势2：OK
3. 按数字按键 3 展示 手势3：点赞

按按键 F 切换至 Keyboard-Mouse Control 模式。（再按 F 切换回）
1. 按键 Q、W、E、R、T 分别控制手指是否弯曲。
2. 鼠标滚轮缩放。
3. 鼠标移动以旋转视角。
