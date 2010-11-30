import errno
import event
import socket

def event_ready(evt, fd, what, conn):
    if what & event.EV_TIMEOUT:
        # the client didn't send something for too long
        print 'Timeout'
        # close the socket...
        conn.close()
        # ...and stop the event loop
        evt.base.loopbreak()
        return
        
    if what & event.EV_READ:
        data = ''
        # get all available data
        while True:
            try:
                add = conn.recv(1024)
                if not add:
                    break
            except socket.error, e:
                # we have non-blocking sockets, so EAGAIN is normal
                if e.args[0] != errno.EAGAIN:
                    raise
                break
            
            data += add
        
        data = data.strip()
        print 'Received:', data
        if data == 'bye':
            # the client wants to close the connection
            print 'Closing'
            # close the socket...
            conn.close()
            # ...and stop the event loop
            evt.base.loopbreak()
            return

def main():
    # the classic hello world example...
    
    # we need an event base that will hold the events
    base = event.Base()
    print 'Initialized base with %r backend' % (base.method)

    # create listening socket
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(('localhost', 1234))
    s.listen(1)
    print 'Running on %s:%d' % s.getsockname()
    
    # wait for client connection
    conn, addr = s.accept()
    conn.setblocking(False)
    print 'Connected from %s:%d' % addr
    
    # add event that waits for available data with a timeout of 5 seconds
    evt = event.Event(base, conn.fileno(), event.EV_READ|event.EV_PERSIST, event_ready, conn)
    evt.add(5)
    
    # run event loop until termination
    base.loop()

if __name__ == '__main__':
    main()
