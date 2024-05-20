# main: main.o conv.o vad.o                                 
# 	gcc -o main main.o conv.o vad.o -lm
# main.o: main.c
# 	gcc -c main.c
# input.o: conv.c
# 	gcc -c conv.c -lm
# add.o: vad.c
# 	gcc -c vad.c
# clean:
# 	rm *.o
# 	rm main

# 设置构建目录的路径
BUILDDIR = build

# 编译和链接你的程序
main: $(BUILDDIR)/main.o $(BUILDDIR)/conv.o $(BUILDDIR)/vad.o
	gcc -o main $(BUILDDIR)/main.o $(BUILDDIR)/conv.o $(BUILDDIR)/vad.o -lm

# 编译你的源文件
$(BUILDDIR)/main.o: main.c
	gcc -c main.c -o $(BUILDDIR)/main.o

$(BUILDDIR)/conv.o: conv.c
	gcc -c conv.c -o $(BUILDDIR)/conv.o

$(BUILDDIR)/vad.o: vad.c
	gcc -c vad.c -o $(BUILDDIR)/vad.o

# 清理你的构建目录
clean:
	rm $(BUILDDIR)/*.o
	rm main