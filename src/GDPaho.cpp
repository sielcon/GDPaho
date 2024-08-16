#include "GDPaho.h"

#include <mutex>

#include <Godot.hpp>
#include "PahoWrapper.hpp"

using namespace godot;

GDPaho::GDPaho() : 
	m_wrapper(nullptr),
	m_connection_initialized_once(false) {
}

GDPaho::~GDPaho() {
	if (m_wrapper) {
		if (m_wrapper->is_connected()) {
			m_wrapper->disconnect()->wait();
		}
		delete m_wrapper;
	}
}

//###############################################################
//	Godot methods
//###############################################################

void GDPaho::_register_methods() {
	// Methods
	register_method("loop", &GDPaho::loop);
	register_method("initialise", &GDPaho::initialise);
	register_method("initialise_full_address", &GDPaho::initialise_full_address);
	register_method("username_pw_set", &GDPaho::username_pw_set);
	register_method("is_connected_to_broker", &GDPaho::is_connected_to_broker);
	register_method("broker_connect", &GDPaho::connect);
	register_method("broker_reconnect", &GDPaho::reconnect);
	register_method("broker_disconnect", &GDPaho::disconnect);
	register_method("publish", &GDPaho::publish);
	register_method("subscribe", &GDPaho::subscribe);
	register_method("unsubscribe", &GDPaho::unsubscribe);
	register_method("unsubscribe_wait", &GDPaho::unsubscribe_wait);
	register_method("reason_code_string", &GDPaho::reason_code_string);

	// Signals
	register_signal<GDPaho>("connected", 
		"reason_code", GODOT_VARIANT_TYPE_INT
	);
	register_signal<GDPaho>("disconnected");
	register_signal<GDPaho>("published", 
		"message_id", GODOT_VARIANT_TYPE_INT
	);
	register_signal<GDPaho>("received", 
		"topic", GODOT_VARIANT_TYPE_STRING,
		"payload", GODOT_VARIANT_TYPE_STRING
	);
	register_signal<GDPaho>("subscribed", 
		"message_id", GODOT_VARIANT_TYPE_INT,
		"topic", GODOT_VARIANT_TYPE_STRING
	);
	register_signal<GDPaho>("unsubscribed", 
		"message_id", GODOT_VARIANT_TYPE_INT,
		"topic", GODOT_VARIANT_TYPE_STRING
	);
	register_signal<GDPaho>("log", 
		"level", GODOT_VARIANT_TYPE_INT,
		"message", GODOT_VARIANT_TYPE_STRING
	);
	register_signal<GDPaho>("error", 
		"message", GODOT_VARIANT_TYPE_STRING,
		"reason_code", GODOT_VARIANT_TYPE_INT
	);
}

void GDPaho::_init() {
}

void GDPaho::loop() {
	//while (true) {
		m_mutex.lock();
		for (const auto& l_data : m_mqtt_data) {
			switch (l_data.m_signal_type) {
				case MQTTSignal::connected:
					emit_signal("connected", l_data.m_rc);
					break;
				case MQTTSignal::disconnected:
					emit_signal("disconnected");
					break;
				case MQTTSignal::published:
					emit_signal("published", l_data.m_id);
					break;
				case MQTTSignal::received:
					emit_signal("received", l_data.m_topic, l_data.m_message);
					break;
				case MQTTSignal::subscribed:
					emit_signal("subscribed", l_data.m_id, l_data.m_topic);
					break;
				case MQTTSignal::unsubscribed:
					emit_signal("unsubscribed", l_data.m_id, l_data.m_topic);
					break;
				case MQTTSignal::log:
					emit_signal("log", l_data.m_level, l_data.m_message);
					break;
				case MQTTSignal::error:
					emit_signal("error", l_data.m_message, l_data.m_rc);
					break;
				default:
					break;
			}
		}
		m_mqtt_data.clear();
		m_mutex.unlock();
	//	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	//}
}

//###############################################################
//	Internals
//###############################################################


//###############################################################
//	Wrapped methods
//###############################################################

int GDPaho::initialise(const String p_id, const String p_host, const String p_port) {
	return initialise_full_address(p_id, "tcp://" + p_host + ":" + p_port);
}

int GDPaho::initialise_full_address(const String p_id, const String p_full_address) {
	try {
		String m_connection_address = p_full_address;
		m_wrapper = new PahoWrapper(*this, p_id.utf8().get_data(), m_connection_address.utf8().get_data());
		return mqtt::ReasonCode::SUCCESS;
	} catch (std::exception& p_exception) {
		return atoi(p_exception.what());
	}
}

int GDPaho::username_pw_set(const String p_username, const String p_password) {
	if (!m_wrapper) {
		return PAHO_ERR_NOT_INIT;
	}
	m_wrapper->username_pw_set(p_username.utf8().get_data(), p_password.utf8().get_data());
	return mqtt::ReasonCode::SUCCESS;
}

bool GDPaho::is_connected_to_broker() {
	if (m_wrapper) {
		return m_wrapper->is_connected();
	}
	return false;
}

int GDPaho::connect(const bool p_clean_session, const int p_keep_alive) {
	if (!m_wrapper) {
		return PAHO_ERR_NOT_INIT;
	}
	if (m_wrapper->is_connected()) {
		return PAHO_ERR_ALREADY_CONNECTED;
	}
	m_connection_initialized_once = true;
	return m_wrapper->connect_to_broker(p_clean_session, p_keep_alive);
}

int GDPaho::reconnect() {
	if (!m_wrapper) {
		return PAHO_ERR_NOT_INIT;
	}
	if (m_wrapper->is_connected()) {
		return PAHO_ERR_ALREADY_CONNECTED;
	}
	if (!m_connection_initialized_once) {
		return PAHO_ERR_CONNECTION_NOT_INITIALIZED_ONCE;
	}
	return m_wrapper->reconnect_to_broker();
}

int GDPaho::disconnect() {
	if (!m_wrapper) {
		return PAHO_ERR_NOT_INIT;
	}
	if (!m_wrapper->is_connected()) {
		return PAHO_ERR_NOT_CONNECTED;
	}
	return m_wrapper->disconnect_to_broker();
}	

int GDPaho::publish(const String p_topic, const String p_payload, const int p_qos, const bool p_retain) {
	if (!m_wrapper) {
		return PAHO_ERR_NOT_INIT;
	}
	if (!m_wrapper->is_connected()) {
		return PAHO_ERR_NOT_CONNECTED;
	}
	return m_wrapper->publish_to(p_topic.utf8().get_data(), p_payload.utf8().get_data(), p_qos, p_retain);
}	

int GDPaho::subscribe(const String p_sub, const int p_qos) {
	if (!m_wrapper) {
		return PAHO_ERR_NOT_INIT;
	}
	if (!m_wrapper->is_connected()) {
		return PAHO_ERR_NOT_CONNECTED;
	}
	return m_wrapper->subscribe_to(p_sub.utf8().get_data(), p_qos);
}	

int GDPaho::unsubscribe(const String p_sub) {
	if (!m_wrapper) {
		return PAHO_ERR_NOT_INIT;
	}
	if (!m_wrapper->is_connected()) {
		return PAHO_ERR_NOT_CONNECTED;
	}
	return m_wrapper->unsubscribe_to(p_sub.utf8().get_data());
}

int GDPaho::unsubscribe_wait(const String p_sub) {
	if (!m_wrapper) {
		return PAHO_ERR_NOT_INIT;
	}
	if (!m_wrapper->is_connected()) {
		return PAHO_ERR_NOT_CONNECTED;
	}
	return m_wrapper->unsubscribe_wait_to(p_sub.utf8().get_data());
}

String GDPaho::reason_code_string(const int p_rc) {
	if (p_rc == PAHO_ERR_NOT_INIT) {
		return "MQTT client is not initialized, initialise() must be call first";
	} else if (p_rc == PAHO_ERR_CONNECTION_NOT_INITIALIZED_ONCE) {
		return "MQTT client is not completely initialized, connect() must be called at least once";
	} else if (p_rc == PAHO_ERR_ALREADY_CONNECTED) {
		return "MQTT client is already connected";
	} else if (p_rc == PAHO_ERR_NOT_CONNECTED) {
		return "MQTT client is not connected";
	}
	return String(mqtt::exception::reason_code_str(p_rc).c_str());
}

//###############################################################
//	Callback
//###############################################################


//###############################################################
//	Signals
//###############################################################

void GDPaho::emit_on_connect(const int p_rc) {
	m_mutex.lock();
	m_mqtt_data.push_back(MQTTData(MQTTSignal::connected, p_rc));
	m_mutex.unlock();
}

void GDPaho::emit_on_disconnect() {
	m_mutex.lock();
	m_mqtt_data.push_back(MQTTData(MQTTSignal::disconnected));
	m_mutex.unlock();
}

void GDPaho::emit_on_publish(const int p_mid) {
	m_mutex.lock();
	m_mqtt_data.push_back(MQTTData(MQTTSignal::published, 0, p_mid));
	m_mutex.unlock();
}

void GDPaho::emit_on_message(const mqtt::const_message_ptr p_message) {
	m_mutex.lock();
	m_mqtt_data.push_back(MQTTData(MQTTSignal::received, 0, 0, 0, String(p_message->to_string().c_str()), String(p_message->get_topic().c_str())));
	m_mutex.unlock();
}

void GDPaho::emit_on_subscribe(const int p_mid, const std::string& p_topic) {
	m_mutex.lock();
	m_mqtt_data.push_back(MQTTData(MQTTSignal::received, 0, p_mid, 0, "", String(p_topic.c_str())));
	m_mutex.unlock();
}

void GDPaho::emit_on_unsubscribe(const int p_mid, const std::string& p_topic) {
	m_mutex.lock();
	m_mqtt_data.push_back(MQTTData(MQTTSignal::unsubscribed, 0, p_mid, 0, "", String(p_topic.c_str())));
	m_mutex.unlock();
}

void GDPaho::emit_on_log(const int p_level, const std::string& p_message) {
	m_mutex.lock();
	m_mqtt_data.push_back(MQTTData(MQTTSignal::log, 0, 0, p_level, String(p_message.c_str())));
	m_mutex.unlock();
}

void GDPaho::emit_on_error(const std::string& p_message, const int p_rc) {
	m_mutex.lock();
	m_mqtt_data.push_back(MQTTData(MQTTSignal::error, p_rc, 0, 0, String(p_message.c_str())));
	m_mutex.unlock();
}
