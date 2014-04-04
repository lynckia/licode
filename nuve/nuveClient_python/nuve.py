import time
import random
import httplib
import json

class Nuve:

    service = None
    key = None
    url = None

    def __init__(self, service, key, url, port):
        self.service = service
        self.key = key
        self.url = url
        self.port = port

    def createRoom(self, name, options, params=None):
        response = self.send('POST', json.dumps({"name": name, "options": options}), "/rooms/", params)
        return response

    def getRooms(self, params=None):
        response = self.send('GET', None, '/rooms/', params)
        return response

    def getRoom(self, room, params=None):
        response = self.send('GET', None, '/rooms/'+room, params)
        return response

    def deleteRoom(self, room, params=None):
        response = self.send('DELETE', None, '/rooms/'+room, params)
        return response

    def createToken(self, room, username, role, params=None):
        response = self.send('POST', json.dumps({}), '/rooms/'+ room + '/tokens', params, username, role)
        return response

    def createService(self, name, key, params=None):
        response = self.send('POST', json.dumps({"name": name, "key": key}), '/services/', params)
        return response

    def getServices(self, params=None):
        response = self.send('GET',  None, '/services/', params)
        return response

    def getService(self, service, params=None):
        response = self.send('GET',  None, '/services/' + service, params)
        return response

    def deleteService(self, service, params=None):
        response = self.send('DELETE',  None, '/services/' + service, params)
        return response

    def getUsers(self, room, params=None):
        response = self.send('GET',  None, '/rooms/' + room + '/users/', params)
        return response

    def getUser(self, room, user, params=None):
        response = self.send('GET',  None, '/rooms/' + room + '/users/' + user, params)
        return response

    def deleteUser(self, room, user, params=None):
        response = self.send('DELETE',  None, '/rooms/' + room + '/users/' + user, params)
        return response


    def send(self, method, body, url, params=None, username="", role=""):
        if (params == None):
            service = self.service
            key = self.key
        else:
            service = params.service
            key = params.key

        timestamp = int(time.time()*1000)
        cnounce = int(random.random()*99999)
        toSign = str(timestamp) + ',' + str(cnounce)

        header = 'MAuth realm=http://marte3.dit.upm.es,mauth_signature_method=HMAC_SHA1'

        if (username != '' and role != ''):
            header += ',mauth_username='
            header +=  username
            header += ',mauth_role='
            header +=  role

            toSign += ',' + username + ',' + role

        signed = self.calculateSignature(toSign, key)

        header += ',mauth_serviceid='
        header +=  service
        header += ',mauth_cnonce='
        header += str(cnounce)
        header += ',mauth_timestamp='
        header +=  str(timestamp)
        header += ',mauth_signature='
        header +=  signed

        conn = httplib.HTTPConnection(self.url, self.port)
        headers = {"Authorization": header, 'Content-Type': 'application/json'}
        conn.request(method, url, body, headers)
        res = conn.getresponse()
        if res.status == 401:
            print res.status, res.reason
            raise Exception('unauthorized')
        response = res.read()
        try:
            data = json.loads(response)
        except Exception:
            data = response
        conn.close()
        return data

    def calculateSignature(self, toSign, key):
        from hashlib import sha1
        import hmac
        import binascii
        hasher = hmac.new(key, toSign, sha1)
        signed = binascii.b2a_base64(hasher.hexdigest())[:-1]
        return signed
