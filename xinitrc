# xinitrc file for testing our window manager.

# 1. Start some programs to play with.
xcalc &
sleep 2 && xterm &

# 2. Start our window manager.
export GLOG_logtostderr=1
exec ./wave_wm
