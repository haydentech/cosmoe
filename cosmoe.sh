# check for the common mistake of not being root
# this will change one day, but for now root is required
if [ `whoami` != "root" ]
then
   echo "ERROR: You must execute this script as root."
   echo "       Read the README.md file for more information."
   exit 1
fi

# Make sure we can find our apps
PATH=$PATH:/usr/local/bin

# Make sure we can find libcosmoe
LIBPATH=$LIBPATH:/usr/local/lib
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib
ulimit -c unlimited

# Avoid issues when working with an address-sanitized build
export ASAN_OPTIONS=detect_odr_violation=0

# remove stale shared memory segments
clean_shm.sh

registrar > registrar.out &
sleep 1

# start appserver, registrar and a demo app
app_server > server.out &
sleep 2

Deskbar > Deskbar.out &
sleep 1

guido > guido.out &
sleep 1

Terminal > terminal.out

# if we arrive here, Cosmoe has terminated.  Try to kill any loose ends.
killall guido
killall Deskbar
killall registrar
killall terminal
killall app_server
sleep 1

# if something won't die normally, try harder to kill it
killall -9 guido
killall -9 Deskbar
killall -9 registrar
killall -9 terminal
killall -9 app_server


# remove stale shared memory segments
clean_shm.sh
