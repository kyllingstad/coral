/**
\file
\brief  Defines the slave RPC protocol.
\copyright
    Copyright 2018-2018, SINTEF Ocean and the Coral contributors.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef CORAL_PROTOCOL_SLAVE_RPC_HPP
#define CORAL_PROTOCOL_SLAVE_RPC_HPP

#include <coral/slave/async_instance.hpp>


namespace coral
{
namespace slave_rpc
{


class SlaveRPCClient : public AsyncInstance
{
public:
    SlaveRPCClient(const coral::net::Endpoint& server);

    folly::SemiFuture<coral::model::SlaveTypeDescription>
        TypeDescription() const override;

    folly::SemiFuture<void> Setup(
        const std::string& slaveName,
        const std::string& executionName,
        coral::model::TimePoint startTime,
        coral::model::TimePoint stopTime,
        bool adaptiveStepSize,
        double relativeTolerance) override;

    folly::SemiFuture<void> StartSimulation() override;

    folly::SemiFuture<void> EndSimulation() override;

    folly::SemiFuture<bool> DoStep(
        coral::model::TimePoint currentT,
        coral::model::TimeDuration deltaT) override;

    folly::SemiFuture<void> GetRealVariables(
        gsl::span<const coral::model::VariableID> variables,
        gsl::span<double> values) const override;

    folly::SemiFuture<void> GetIntegerVariables(
        gsl::span<const coral::model::VariableID> variables,
        gsl::span<int> values) const override;

    folly::SemiFuture<void> GetBooleanVariables(
        gsl::span<const coral::model::VariableID> variables,
        gsl::span<bool> values) const override;

    folly::SemiFuture<void> GetStringVariables(
        gsl::span<const coral::model::VariableID> variables,
        gsl::span<std::string> values) const override;

    folly::SemiFuture<bool> SetRealVariables(
        gsl::span<const coral::model::VariableID> variables,
        gsl::span<const double> values) override;

    folly::SemiFuture<bool> SetIntegerVariables(
        gsl::span<const coral::model::VariableID> variables,
        gsl::span<const int> values) override;

    folly::SemiFuture<bool> SetBooleanVariables(
        gsl::span<const coral::model::VariableID> variables,
        gsl::span<const bool> values) override;

    folly::SemiFuture<bool> SetStringVariables(
        gsl::span<const coral::model::VariableID> variables,
        gsl::span<const std::string> values) override;

private:
    class Impl;
    std::unique_ptr<Impl> m_pimpl;
};


}}
#endif // header guard

