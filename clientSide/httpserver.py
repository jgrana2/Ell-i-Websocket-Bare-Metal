import SimpleHTTPServer, SocketServer
import urlparse

PORT = 8080


class MyHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):
   def do_GET(self):

       # Parse query data & params to find out what was passed
       parsedParams = urlparse.urlparse(self.path)
       queryParsed = urlparse.parse_qs(parsedParams.query)
       # print parsedParams
       # request is either for a file to be served up or our test
       if parsedParams.path == "/test":
          self.processMyRequest(queryParsed)
       else:
          # Default to serve up a local file 
          SimpleHTTPServer.SimpleHTTPRequestHandler.do_GET(self);

   def processMyRequest(self, query):

       self.send_response(200)
       self.send_header('Content-Type', 'text/html')
       self.end_headers()
       self.wfile.write("<?xml version='1.0'?>")
       self.wfile.write("<sample>")
       self.wfile.write("value")
       self.wfile.write("</sample>")
       self.wfile.close()
       print "ok"

Handler = MyHandler

httpd = SocketServer.TCPServer(("", PORT), Handler)

print "serving at port", PORT
httpd.serve_forever()