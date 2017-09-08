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

#include <coral/model.hpp>


namespace coral
{
namespace master
{


class ModelBuilder
{
public:
    ModelBuilder() noexcept;

    void AddSlave(
        const std::string& name,
        const coral::model::SlaveTypeDescription& type);

    void SetInitialValue(
        const std::string& slaveName,
        const std::string& variableName,
        const coral::model::ScalarValue& value);

    void ResetInitialValue(
        const std::string& slaveName,
        const std::string& variableName);

    void ConnectVariables(
        const std::string& sourceSlaveName,
        const std::string& sourceVariableName,
        const std::string& targetSlaveName,
        const std::string& targetVariableName);

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
#endif // header guard
