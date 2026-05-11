
set -e

function build_all () {
  build_elma
  build_node
  build_ts
}

function build_elma () {
  (
    cd elma
    [ -d ./build ] || meson setup build -Dbuildtype=release
    meson compile -C build
  )
}

function build_node () {
  (
    `which yarn` || npm install -g yarn
    yarn
    [ -d ./build ] || node-gyp configure
    node-gyp build --debug
    cd node_modules
    rm elma.node
    ln -s ../build/Debug/elma.node
  )
}

function build_ts () {
  npx esbuild src/main.ts --bundle --outfile=main.js --platform=node \
    --external:elma.node
}


function main_gdb () {
#gdb --args node script.js
#(gdb) set pagination off
  gdb \
    -ex 'handle SIGSEGV stop always' \
    -ex 'handle SIGABRT stop always' \
    -ex 'catch syscall 60' \
    -ex run --args node main.js $@
}

function main () {
  node main.js $@
}

function dev () {
  npx tsx src/main.ts $@
  #valgrind --tool=callgrind node ts/test.js
}



cd `dirname $0`
export LD_LIBRARY_PATH="./elma/build/"


if [ -z "$1" ]; then
  echo "Commands:"
  echo
  cat $0 | sed -rne 's/^function ([^_][^ \(]+).*/  \1/p'
  echo
else

  set -a # auto export variables
  touch .env.local
  . .env.local
  set +a # end auto export

  cmd=$1           # Get the function name from argv
  shift            # Remove function name
  eval $cmd $@     # Call function and parse arguments
fi

