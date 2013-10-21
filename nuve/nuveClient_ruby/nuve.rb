require 'net/http'
require 'base64'
require 'hmac-sha1'

class Nuve

	def initialize (service, key, url)
		@service = service
		@key = key
		@url = url
	end

	def getRooms ()
		return send('GET', nil, 'rooms', '', '')
	end

    def createRoom (name, options={})
    	return send('POST', {'name' => name, 'options' => options}, 'rooms', '', '')
    end

    def getRoom (room)
 		return send('GET', nil, 'rooms/' + room, '', '')
    end

    def deleteRoom (room)
		return send('DELETE', nil, 'rooms/' + room, '', '')
    end

    def createToken (room, username, role)
    	return send('POST', nil, 'rooms/' + room + '/tokens', username, role)
    end

    def createService (name, key)
    	return send('POST', {'name' => name, 'key' => key}, 'services/', '', '');
    end

    def getServices ()
		return send('GET', nil, 'services/', '', '');
    end

    def getService (service)
   		return send('GET', nil, 'services/' + service, '', '');
    end

    def deleteService (service)
    	return send('DELETE', nil, 'services/' + service, '', '');
    end

    def getUsers (room)
    	return send('GET', nil, 'rooms/' + room + '/users/', '', '');
    end

	private
	def send (method, body, url, username, role)

		uri = URI(@url + url)

		header = 'MAuth realm=http://marte3.dit.upm.es,mauth_signature_method=HMAC_SHA1'

		timestamp = (Time.now.to_f * 1000).to_i
		cnounce = rand(99999)

		toSign = timestamp.to_s + "," + cnounce.to_s

		if username != '' && role != ''

			header += ',mauth_username='
            header +=  username
            header += ',mauth_role='
            header +=  role

            toSign += ',' + username + ',' + role

		end

		signed = calculateSignature(toSign, @key)

		header += ',mauth_serviceid='
        header +=  @service
        header += ',mauth_cnonce='
        header += cnounce.to_s
        header += ',mauth_timestamp='
        header +=  timestamp.to_s
        header += ',mauth_signature='
        header +=  signed

        if body != nil
        	request = Net::HTTP::Post.new(uri.request_uri,  {'Authorization' => header, 'Content-Type' => 'application/json'})
        	request.set_form_data(body)

        else
        	if method == 'GET'
        		request = Net::HTTP::Get.new(uri.request_uri,  {'Authorization' => header})
        	elsif method == 'POST'
        		request = Net::HTTP::Post.new(uri.request_uri,  {'Authorization' => header})
        	else
        		request = Net::HTTP::Delete.new(uri.request_uri,  {'Authorization' => header})
        	end
        end

		Net::HTTP.start(uri.host, uri.port) do |http|
		  response = http.request(request)
		  return response.body
		end

	end

	private
	def calculateSignature (toSign, key)
		hmac = HMAC::SHA1.new(key)
		hex = hmac.update(toSign)
		signed   = Base64.encode64("#{hex}")
		return signed
	end

end
