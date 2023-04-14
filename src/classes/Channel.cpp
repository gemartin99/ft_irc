#include "Log.hpp"

#include "Channel.hpp"

ft_irc::Channel::Channel(void) :
	_name(),
	_topic(),
	_key(),
	_clients(),
	_mode(),
	_client_limit()
{
	LOG_DEBUG("Creating new channel");
}

ft_irc::Channel::Channel(const std::string& name) :
	_topic(),
	_key(),
	_mode(),
	_client_limit()
{
	LOG_DEBUG("Creating new channel")
	this->setName(name);
	LOG_INFO("New channel created: " << name)
}

ft_irc::Channel::Channel(const ft_irc::Channel& c) :
	_name(c._name),
	_topic(c._topic),
	_key(c._key),
	_clients(c._clients),
	_mode(c._mode),
	_client_limit(c._client_limit)
{}

ft_irc::Channel&
ft_irc::Channel::operator=(const ft_irc::Channel& c) {
	this->_name = c._name;
	this->_topic = c._topic;
	this->_key = c._key;
	this->_clients = c._clients;
	this->_mode = c._mode;
	this->_client_limit = c._client_limit;
	return *this;
}

ft_irc::Channel::~Channel(void) {
	LOG_INFO("Removed channel: " << this->_name)
}

const std::string&
ft_irc::Channel::getName(void) const {
	return this->_name;
}

const std::string&
ft_irc::Channel::getTopic(void) const {
	return this->_topic;
}

const std::string&
ft_irc::Channel::getKey(void) const {
	return this->_key;
}

void
ft_irc::Channel::setName(const std::string& name) {
	if (name.length() == 0)
		LOG_WARN("setName called with empty string")
		throw std::invalid_argument("Name must be a non empty string");
	if ((name.at(0) != '#' && name.at(0) != '&')
		|| name.find(' ') != std::string::npos
		|| name.find(0x07) != std::string::npos
		|| name.find(',') != std::string::npos)
		LOG_WARN("setName called with an invalid character")
		throw ft_irc::Channel::InvalidChannelName("Invalid channel  name: " + name);
	LOG_INFO("Channel's name changed from: " << this->_name << " to: " << name)
	this->_name = name;
}

void
ft_irc::Channel::setTopic(ft_irc::Client& source, std::string& topic) {
	if (!this->_clients.count(source.getFd()))
		LOG_WARN("setTopic called with a client not on channel")
		throw ft_irc::Channel::NotOnChannel();
	if (this->_mode & PROTECTED_TOPIC
		&& !(this->_clients.at(source.getFd()).mode & OPERATOR)
		&& !(this->_clients.at(source.getFd()).mode & HALFOP)
		&& !(this->_clients.at(source.getFd()).mode & PROTECTED))
		LOG_WARN("setTopic called with a client with no privileges")
		throw ft_irc::Channel::NoPrivsOnChannel();
	LOG_INFO("Channel's topic changed from: " << this->_topic << " to: " << topic)
	this->_topic = topic;
}

void
ft_irc::Channel::setKey(std::string& key) {
	if (key.length() == 0)
		LOG_WARN("setKet with an empty string")
		throw std::invalid_argument("Key must be a non empty string");
	LOG_INFO("Channel's key changed from: " << this->_key << " to: " << key)
	this->_key = key;
}

void
ft_irc::Channel::setClientLimit(long limit) {
	if (limit <= 0)
		LOG_WARN("setClientLimit with an invalid number: " << limit)
		throw ft_irc::Channel::InvalidLimit("Invalid limit");
	LOG_INFO("Channel's limit changed from: " << this->_client_limit << " to: " << limit)
	this->_client_limit = limit;
}

void
ft_irc::Channel::toggleMode(const char mode) {
	if (mode > (INVITE|MODERATE|SECRET|PROTECTED_TOPIC|NOT_EXTERNAL_MSGS)
		|| mode <= 0)
		LOG_WARN("setClientLimit with an invalid mode: " << mode)
		throw ft_irc::Channel::InvalidMode("Invalid mode");
	this->_mode ^= mode;
}

bool
ft_irc::Channel::isInChannel(const ft_irc::Client& client) {
	return this->_clients.count(client.getFd());
}

bool
ft_irc::Channel::isInChannel(const std::string& nickname) {
	for (client_iterator it = this->_clients.begin(); it != this->_clients.end(); it++)
		if (it->second.client.getNickname() == nickname)
			LOG_INFO("isInChannel client in channel: " << nickname)
			return true;
	LOG_INFO("isInChannel client not in channel: " << nickname)
	return false;
}

bool
ft_irc::Channel::addClient(const ft_irc::Client& client) {
	if (this->_clients.count(client.getFd()))
		LOG_WARN("addClient client already on channel: " << client.getNickname())
		return false;
	this->_clients.insert(std::make_pair<int, ft_irc::Channel::ClientInfo>(client.getFd(),
		ft_irc::Channel::ClientInfo(client)));
	LOG_INFO("New client added to channel: " << client.getNickname())
	if (this->_clients.size() == 1)
		LOG_INFO("addClient first client in channel: " << client.getNickname())
		this->_clients.begin()->second.mode = (FOUNDER|OPERATOR);
	return true;
}

bool
ft_irc::Channel::banMask(const std::string& mask) {
	if (this->_masks.count(mask)) {
		if (this->_masks[mask] & BAN)
			LOG_WARN("banMask mask already banned in channel: " << mask)
			return false;
		this->_masks[mask] ^= BAN;
	}
	else
		this->_masks.insert(std::make_pair<std::string, mask_mode>(mask, BAN));
	LOG_INFO("Mask banned from channel: " << mask)
	return true;
}

bool
ft_irc::Channel::invite(const Client& source, const std::string& nick) {
	if (!this->_clients.count(source.getFd()))
		LOG_WARN("invite, source not on channel: " << source.getNickname())
		throw ft_irc::Channel::NotOnChannel();
	if (this->_mode & INVITE_ONLY && !(this->_clients.at(source.getFd()).mode & OPERATOR))
		LOG_WARN("invite, source doesent have privileges: " << source.getNickname())
		throw ft_irc::Channel::NoPrivsOnChannel();
	if (this->isInChannel(nick))
		LOG_WARN("invite, client already on channel: " << nick)
		throw ft_irc::Channel::AlreadyOnChannel();
	if (this->_masks.count(nick)) {
		if (this->_masks[nick] & INVITE)
			LOG_WARN("invite, client already invited to channel: " << nick)
			return false;
		this->_masks[nick] ^= INVITE;
	}
	else
		this->_masks.insert(std::make_pair<std::string, mask_mode>(nick, INVITE));
	LOG_INFO("invite, client invited to channel: " << nick)
	return true;
}

bool
ft_irc::Channel::join(const ft_irc::Client& client, const std::string& key) {
	if (this->_clients.count(client.getFd()))
		LOG_WARN("join, client already in channel: " << client.getNickname())
		return false;
	if (this->_masks.count(client.getMask()) && this->_masks[client.getMask()] & BAN)
		LOG_WARN("join, client banned from channel: " << client.getNickname())
		throw ft_irc::Channel::BannedClient();
	if (this->_mode & INVITE_ONLY &&
		(!this->_masks.count(client.getMask()) || !(this->_masks[client.getMask()] & INVITE)))
		LOG_WARN("join, client not invited to channel: " << client.getNickname())
		throw ft_irc::Channel::InviteOnlyChannel();
	if (this->_client_limit != 0 && this->_client_limit >= this->_clients.size())
		LOG_WARN("join, client can't join, channel is full: " << client.getNickname())
		throw ft_irc::Channel::ChannelIsFull();
	if (!this->_key.empty() && this->_key != key)
		LOG_WARN("join, client " << client.getNickname() << " can't join, invalid key: " << key)
		throw ft_irc::Channel::InvalidKey("Incorrect channel key");
	LOG_INFO("Client joined to channel: " << client.getNickname())
	this->addClient(client);
	return true;
}

ft_irc::Channel::InvalidChannelName::InvalidChannelName(std::string msg) : std::invalid_argument(msg)
{}

ft_irc::Channel::InvalidMode::InvalidMode(std::string msg) : std::invalid_argument(msg)
{}

ft_irc::Channel::InvalidKey::InvalidKey(std::string msg) : std::invalid_argument(msg)
{}

ft_irc::Channel::InvalidLimit::InvalidLimit(std::string msg) : std::invalid_argument(msg)
{}

ft_irc::Channel::BannedClient::BannedClient() {}

ft_irc::Channel::InviteOnlyChannel::InviteOnlyChannel() {}

ft_irc::Channel::ChannelIsFull::ChannelIsFull() {}

ft_irc::Channel::NotOnChannel::NotOnChannel() {}

ft_irc::Channel::NoPrivsOnChannel::NoPrivsOnChannel() {}

ft_irc::Channel::AlreadyOnChannel::AlreadyOnChannel() {}

ft_irc::Channel::ClientInfo::ClientInfo(const ft_irc::Client& c) :
	client(c),
	mode()
{}
