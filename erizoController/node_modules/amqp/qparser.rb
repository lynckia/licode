#!/usr/bin/env ruby
# USAGE: ruby qparser.rb amqp-0.8.xml > amqp-definitions-0-8.js

require 'rubygems'
require 'nokogiri'
require 'erb'
require 'pathname'
require 'yaml'
require 'active_support'
require 'json'

class InputError < StandardError; end

def spec_details(doc)
  # AMQP spec details

  spec_details = {}

  root = doc.at('amqp')
  spec_details['major'] = root['major']
  spec_details['minor'] = root['minor']
  spec_details['revision'] = root['revision'] || '0'
  spec_details['port'] = root['port']
  spec_details['comment'] = root['comment'] || 'No comment'

  spec_details
end

def jscase(s)
  t = s.gsub(/\s|-/, '_').camelcase
  t[0] = t[0].downcase
  t
end

def process_constants(doc)
  # AMQP constants

  constants = {}

  doc.xpath('//constant').each do |element|
    constants[element['value'].to_i] = jscase(element['name'])
  end

  constants.sort
end

def domain_types(doc, major, minor, revision)
  # AMQP domain types

  dt_arr = []
  doc.xpath('amqp/domain').each do |element|
     dt_arr << element['type']
  end

  # Add domain types for specific document
  add_arr = add_types(major, minor, revision)
  type_arr = dt_arr + add_arr

  # Return sorted array
  type_arr.uniq.sort
end

def classes(doc, major, minor, revision)
  # AMQP classes

  cls_arr = []

  doc.xpath('amqp/class').each do |element|
    cls_hash = {}
    cls_hash[:name] = jscase(element['name'])
    cls_hash[:index] = element['index'].to_i
    # Get fields for class
    field_arr = fields(doc, element)
    cls_hash[:fields] = field_arr
    # Get methods for class
    meth_arr = class_methods(doc, element)
    # Add missing methods
    add_arr =[]
    add_arr = add_methods(major, minor, revision) if cls_hash[:name] == 'queue'
    method_arr = meth_arr + add_arr
    # Add array to class hash
    cls_hash[:methods] = method_arr
    cls_arr << cls_hash
  end

  # Return class information array
  cls_arr
end

def class_methods(doc, cls)
  meth_arr = []

  # Get methods for class
  cls.xpath('./method').each do |method|
    meth_hash = {}
    meth_hash[:name] = jscase(method['name'])
    meth_hash[:index] = method['index'].to_i
    # Get fields for method
    field_arr = fields(doc, method)
    meth_hash[:fields] = field_arr
    meth_arr << meth_hash
  end

  # Return methods
  meth_arr
end

def fields(doc, element)
  field_arr = []

  # Get fields for element
  element.xpath('./field').each do |field|
    field_hash = {}
    field_hash[:name] = jscase(field['name'].tr(' ', '-'))
    field_hash[:domain] = field['type'] || field['domain']

    # Convert domain type if necessary
    conv_arr = convert_type(field_hash[:domain])
    field_hash[:domain] = conv_arr[field_hash[:domain]] unless conv_arr.empty?

    field_arr << field_hash
  end

  #  Return fields
  field_arr

end

def add_types(major, minor, revision)
  type_arr = []
  type_arr = ['long', 'longstr', 'octet', 'timestamp'] if (major == '8' and minor == '0' and revision == '0')
  type_arr
end

def add_methods(major, minor, revision)
  meth_arr = []

  if (major == '8' and minor == '0' and revision == '0')
    # Add Queue Unbind method
    meth_hash = {:name => 'unbind',
                 :index => '50',
                 :fields => [{:name => 'ticket', :domain => 'short'},
                             {:name => 'queue', :domain => 'shortstr'},
                             {:name => 'exchange', :domain => 'shortstr'},
                             {:name => 'routing_key', :domain => 'shortstr'},
                             {:name => 'arguments', :domain => 'table'}
                            ]
                }

    meth_arr << meth_hash

    # Add Queue Unbind-ok method
    meth_hash = {:name => 'unbind-ok',
                 :index => '51',
                 :fields => []
                }

    meth_arr << meth_hash
  end

  # Return methods
  meth_arr

end

def convert_type(name)
  type_arr = @type_conversion.reject {|k,v| k != name}
end

# Start of Main program

# Read in the spec file
doc = Nokogiri::XML(File.new(ARGV[0]))

# Declare type conversion hash
@type_conversion = {'path' => 'shortstr',
                    'known hosts' => 'shortstr',
                    'known-hosts' => 'shortstr',
                    'reply code' => 'short',
                    'reply-code' => 'short',
                    'reply text' => 'shortstr',
                    'reply-text' => 'shortstr',
                    'class id' => 'short',
                    'class-id' => 'short',
                    'method id' => 'short',
                    'method-id' => 'short',
                    'channel-id' => 'longstr',
                    'access ticket' => 'short',
                    'access-ticket' => 'short',
                    'exchange name' => 'shortstr',
                    'exchange-name' => 'shortstr',
                    'queue name' => 'shortstr',
                    'queue-name' => 'shortstr',
                    'consumer tag' => 'shortstr',
                    'consumer-tag' => 'shortstr',
                    'delivery tag' => 'longlong',
                    'delivery-tag' => 'longlong',
                    'redelivered' => 'bit',
                    'no ack' => 'bit',
                    'no-ack' => 'bit',
                    'no local' => 'bit',
                    'no-local' => 'bit',
                    'peer properties' => 'table',
                    'peer-properties' => 'table',
                    'destination' => 'shortstr',
                    'duration' => 'longlong',
                    'security-token' => 'longstr',
                    'reject-code' => 'short',
                    'reject-text' => 'shortstr',
                    'offset' => 'longlong',
                    'no-wait' => 'bit',
                    'message-count' => 'long'
                   }

# Spec details
spec_info = spec_details(doc)

# Constants
constants = process_constants(doc)


# Frame constants
#frame_constants = constants[0].select {|k,v| k <= 8}
#frame_footer = constants[0].select {|k,v| v == 'End'}[0][0]

# Other constants
other_constants = constants[1]

# Domain types
data_types = domain_types(doc, spec_info['major'], spec_info['minor'], spec_info['revision'])

# Classes
class_defs = classes(doc, spec_info['major'], spec_info['minor'], spec_info['revision'])


def format_name(name)
  name.split('-').collect {|x| x.camelcase }.join
end


puts "exports.constants = " + constants.to_json + ";"
puts "exports.classes = " + class_defs.to_json + ";"
