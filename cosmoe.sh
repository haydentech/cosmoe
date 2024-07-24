# check for the common mistake of not being root
# this will change one day, but for now root is required
if [ `whoami` != "root" ]
then
   echo "ERROR: You must execute this script as root."
   echo "       Read the README.md file for more information."
   exit 1
fi

PATH=$PATH:/usr/local/bin
LIBPATH=$LIBPATH:/usr/local/lib
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib
ulimit -c unlimited

export ASAN_OPTIONS=detect_odr_violation=0

# remove stale shared memory segments
clean_shm.sh

# start appserver, registrar and a demo app
app_server > server.out &
sleep 2

registrar > registrar.out &
sleep 1

guido > guido.out &
sleep 1

terminal > terminal.out

# if we arrive here, Cosmoe has terminated.  Try to kill any loose ends.
killall guido
killall registrar
killall app_server
killall terminal
sleep 1

# if something won't die normally, try harder to kill it
killall -9 guido
killall -9 registrar
killall -9 app_server
killall -9 terminal

# remove stale shared memory segments
clean_shm.sh
