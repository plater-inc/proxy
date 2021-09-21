import subprocess
import sys
import os
import threading
import queue
import os, tempfile


def get_line_from_queue_and_assert(debug_queue, expected_value):
    line = debug_queue.get()
    assert line == expected_value, "Expected line: %sGot: %s" % (expected_value, line)


def run(command, args=[]):
    tmpdir = tempfile.mkdtemp()
    filename = os.path.join(tmpdir, "debug_fifo")
    os.mkfifo(filename)

    def enqueue_output(debug_queue):
        fifo = open(filename, "r")
        for line in iter(fifo.readline, b""):
            if len(line) == 0:
                break
            debug_queue.put(line)
        fifo.close()

    proc = subprocess.Popen(
        [command, filename] + args, stdout=sys.stdout, stderr=sys.stderr
    )
    debug_queue = queue.Queue()
    thread = threading.Thread(target=enqueue_output, args=(debug_queue,))
    thread.daemon = True  # thread dies with the program
    thread.start()

    return (debug_queue, proc)
