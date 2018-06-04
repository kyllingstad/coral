/**
\file
\brief  Defines the coral::slave::AsyncInstance interface.
\copyright
    Copyright 2018-2018, SINTEF Ocean and the Coral contributors.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef CORAL_SLAVE_ASYNC_INSTANCE_HPP
#define CORAL_SLAVE_ASYNC_INSTANCE_HPP

#include <string>
#include <gsl/span>
#include <folly/Future.h>
#include <coral/model.hpp>

namespace coral
{
namespace slave
{


class AsyncInstance
{
public:
    /// Returns an object that describes the slave type.
    virtual folly::SemiFuture<coral::model::SlaveTypeDescription>
        TypeDescription() const = 0;

    /**
    \brief  Instructs the slave to perform pre-simulation setup and enter
            initialisation mode.

    This function is called when the slave has been added to an execution.
    The arguments `startTime` and `stopTime` represent the time interval inside
    which the slave's model equations are required to be valid.  (In other
    words, it is guaranteed that DoStep() will never be called with a time point
    outside this interval.)

    \param [in] slaveName
        The name of the slave in the current execution.  May be empty if this
        feature is not used.
    \param [in] executionName
        The name of the current execution.  May be empty if this feature is
        not used.
    \param [in] startTime
        The earliest possible time point for the simulation.
    \param [in] stopTime
        The latest possible time point for the simulation.  May be infinity if
        there is no defined stop time.
    \param [in] adaptiveStepSize
        Whether the step size is being controlled by error estimation.
    \param [in] relativeTolerance
        Only used if `adaptiveStepSize == true`, and then contains the relative
        tolerance of the step size controller.  The slave may then use this for
        error estimation in its internal integrator.
    */
    virtual folly::SemiFuture<void> Setup(
        const std::string& slaveName,
        const std::string& executionName,
        coral::model::TimePoint startTime,
        coral::model::TimePoint stopTime,
        bool adaptiveStepSize,
        double relativeTolerance) = 0;

    /**
    \brief  Informs the slave that the initialisation stage ends and the
            simulation begins.
    */
    virtual folly::SemiFuture<void> StartSimulation() = 0;

    /// Informs the slave that the simulation run has ended.
    virtual folly::SemiFuture<void> EndSimulation() = 0;

    /**
    \brief  Performs model calculations for the time step which starts at
            the time point `currentT` and has a duration of `deltaT`.

    If this is not the first time step, it can be assumed that the previous
    time step ended at `currentT`.  It can also be assumed that `currentT` is
    greater than or equal to the start time, and `currentT+deltaT` is less than
    or equal to the stop time, specified in the Setup() call.

    \returns
        `true` if the model calculations for the given time step were
        successfully carried out, or `false` if they were not because the
        time step was too long.

    \note
        Currently, retrying a failed time step is not supported, but this is
        planned for a future version.
    */
    virtual folly::SemiFuture<bool> DoStep(
        coral::model::TimePoint currentT,
        coral::model::TimeDuration deltaT) = 0;

    /**
    \brief  Retrieves the values of real variables.

    On return, the `values` array will be filled with the values of the
    variables specified in `variables`, in the same order.

    \pre `variables.size() == values.size()`
    */
    virtual folly::SemiFuture<void> GetRealVariables(
        gsl::span<const coral::model::VariableID> variables,
        gsl::span<double> values) const = 0;

    /**
    \brief  Retrieves the values of integer variables.

    On return, the `values` array will be filled with the values of the
    variables specified in `variables`, in the same order.

    \pre `variables.size() == values.size()`
    */
    virtual folly::SemiFuture<void> GetIntegerVariables(
        gsl::span<const coral::model::VariableID> variables,
        gsl::span<int> values) const = 0;

    /**
    \brief  Retrieves the values of boolean variables.

    On return, the `values` array will be filled with the values of the
    variables specified in `variables`, in the same order.

    \pre `variables.size() == values.size()`
    */
    virtual folly::SemiFuture<void> GetBooleanVariables(
        gsl::span<const coral::model::VariableID> variables,
        gsl::span<bool> values) const = 0;

    /**
    \brief  Retrieves the values of string variables.

    On return, the `values` array will be filled with the values of the
    variables specified in `variables`, in the same order.

    \pre `variables.size() == values.size()`
    */
    virtual folly::SemiFuture<void> GetStringVariables(
        gsl::span<const coral::model::VariableID> variables,
        gsl::span<std::string> values) const = 0;

    /**
    \brief  Sets the values of real variables.

    This will set the value of each variable specified in the `variables`
    array to the value given in the corresponding element of `values`.

    \returns
        `true` if successful and `false` if one or more values were invalid
        (e.g. out of range for the given variables).

    \pre `variables.size() == values.size()`
    */
    virtual folly::SemiFuture<bool> SetRealVariables(
        gsl::span<const coral::model::VariableID> variables,
        gsl::span<const double> values) = 0;

    /**
    \brief  Sets the values of integer variables.

    This will set the value of each variable specified in the `variables`
    array to the value given in the corresponding element of `values`.

    \returns
        `true` if successful and `false` if one or more values were invalid
        (e.g. out of range for the given variables).

    \pre `variables.size() == values.size()`
    */
    virtual folly::SemiFuture<bool> SetIntegerVariables(
        gsl::span<const coral::model::VariableID> variables,
        gsl::span<const int> values) = 0;

    /**
    \brief  Sets the values of boolean variables.

    This will set the value of each variable specified in the `variables`
    array to the value given in the corresponding element of `values`.

    \returns
        `true` if successful and `false` if one or more values were invalid
        (e.g. out of range for the given variables).

    \pre `variables.size() == values.size()`
    */
    virtual folly::SemiFuture<bool> SetBooleanVariables(
        gsl::span<const coral::model::VariableID> variables,
        gsl::span<const bool> values) = 0;

    /**
    \brief  Sets the values of string variables.

    This will set the value of each variable specified in the `variables`
    array to the value given in the corresponding element of `values`.

    \returns
        `true` if successful and `false` if one or more values were invalid
        (e.g. out of range for the given variables).

    \pre `variables.size() == values.size()`
    */
    virtual folly::SemiFuture<bool> SetStringVariables(
        gsl::span<const coral::model::VariableID> variables,
        gsl::span<const std::string> values) = 0;

    // Because it's an interface:
    virtual ~Instance() { }
};


}}
#endif // header guard
