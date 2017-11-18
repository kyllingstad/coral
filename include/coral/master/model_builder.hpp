/**
\file
\brief Defines the coral::master::ModelBuilder class and related functionality.
\copyright
    Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef CORAL_MASTER_MODEL_BUILDER_HPP
#define CORAL_MASTER_MODEL_BUILDER_HPP

#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <coral/config.h>
#include <coral/master/cluster.hpp>
#include <coral/master/execution.hpp>
#include <coral/model.hpp>


namespace coral
{
namespace master
{

class QualifiedVariableName
{
public:
    QualifiedVariableName(const std::string& slave, const std::string& variable);

    const std::string& Slave() const;

    const std::string& Variable() const;

    std::string ToString() const;

    static QualifiedVariableName FromString(const std::string& s);

    bool operator==(const QualifiedVariableName& other) const CORAL_NOEXCEPT;

    bool operator!=(const QualifiedVariableName& other) const CORAL_NOEXCEPT;

private:
    std::string m_slave;
    std::string m_variable;
};


class ModelBuilder
{
public:
    ModelBuilder() CORAL_NOEXCEPT;

    void AddSlave(
        const std::string& name,
        const coral::model::SlaveTypeDescription& type);

    void SetInitialValue(
        const QualifiedVariableName& variable,
        const coral::model::ScalarValue& value);

    const coral::model::ScalarValue& GetInitialValue(
        const QualifiedVariableName& variable) const;

    void ResetInitialValue(const QualifiedVariableName& variable);

    void Connect(
        const QualifiedVariableName& source,
        const QualifiedVariableName& target);

    std::vector<std::tuple<QualifiedVariableName, QualifiedVariableName>>
        GetConnections() const;

    std::vector<QualifiedVariableName> GetUnconnectedInputs() const;

private:
    class Impl;
    std::unique_ptr<Impl, void (*)(Impl*)> m_impl;
};


class ModelConstructionException : public std::runtime_error
{
public:
    ModelConstructionException(const std::string& msg)
        : std::runtime_error{msg}
    { }
};


class EntityNotFoundException : public std::runtime_error
{
public:
    EntityNotFoundException(const std::string& msg)
        : std::runtime_error{msg}
    { }
};


}} // namespace


namespace std
{

template<>
struct hash<coral::master::QualifiedVariableName>
{
    std::size_t operator()(const coral::master::QualifiedVariableName& name)
        const CORAL_NOEXCEPT
    {
        const auto h1 = std::hash<std::string>{}(name.Slave());
        const auto h2 = std::hash<std::string>{}(name.Variable());
        return h1 ^ (h2 << 1);
    }
};

}
#endif // header guard
