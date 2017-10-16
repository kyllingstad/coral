#include <coral/master/model_builder.hpp>

#include <cassert>
#include <coral/error.hpp>


namespace coral
{
namespace master
{


namespace
{
    std::string QualifiedVariableNameString(
        const std::string& slave,
        const std::string& variable)
    {
        auto s = slave;
        s += '.';
        s += variable;
        return s;
    }
}


// =============================================================================
// QualifiedVariableName
// =============================================================================

QualifiedVariableName::QualifiedVariableName(
    const std::string& slave,
    const std::string& variable)
    : m_slave(slave)
    , m_variable(variable)
{
    CORAL_INPUT_CHECK(!slave.empty());
    CORAL_INPUT_CHECK(!variable.empty());
}


const std::string& QualifiedVariableName::Slave() const
{
    return m_slave;
}


const std::string& QualifiedVariableName::Variable() const
{
    return m_variable;
}


std::string QualifiedVariableName::ToString() const
{
    return QualifiedVariableNameString(m_slave, m_variable);
}


QualifiedVariableName QualifiedVariableName::FromString(const std::string& s)
{
    const auto pos = s.find('.');
    if (pos < 1 || pos >= s.size()-1) {
        throw std::invalid_argument("Not a fully qualified variable name: " + s);
    }
    return QualifiedVariableName{s.substr(0, pos), s.substr(pos+1)};
}


bool QualifiedVariableName::operator==(const QualifiedVariableName& other)
    const CORAL_NOEXCEPT
{
    return m_slave == other.m_slave && m_variable == other.m_variable;
}


bool QualifiedVariableName::operator!=(const QualifiedVariableName& other)
    const CORAL_NOEXCEPT
{
    return !operator==(other);
}


// =============================================================================
// ModelBuilder
// =============================================================================


namespace
{
    struct CachedSlaveType
    {
        explicit CachedSlaveType(const coral::model::SlaveTypeDescription& d)
            : description(d)
        {
            for (const auto& v : description.Variables()) {
                variables.insert(std::make_pair(v.Name(), &v));
            }
        }

        coral::model::SlaveTypeDescription description;
        std::unordered_map<std::string, const coral::model::VariableDescription*> variables;
    };


    std::string ConnectionErrMsg(
        const std::string& sourceSlaveName,
        const std::string& sourceVariableName,
        const std::string& targetSlaveName,
        const std::string& targetVariableName,
        const std::string& details)
    {
        return "Cannot connect variable "
            + QualifiedVariableNameString(sourceSlaveName, sourceVariableName)
            + " to "
            + QualifiedVariableNameString(targetSlaveName, targetVariableName)
            + ": " + details;
    }


    // Returns vacuously if `value` is a valid value for `variable`,
    // otherwise throws an exception with an explanatory message.
    // The slave name is only used in error messages.
    void EnforceValidValue(
        const std::string& slaveName,
        const coral::model::VariableDescription& variable,
        const coral::model::ScalarValue& value)
    {
        if (coral::model::DataTypeOf(value) != variable.DataType()) {
            throw ModelConstructionException{
                "Attempted to assign a value of type "
                + coral::model::DataTypeName(coral::model::DataTypeOf(value))
                + " to variable "
                + QualifiedVariableNameString(slaveName, variable.Name())
                + " which has type "
                + coral::model::DataTypeName(variable.DataType())};
        }
        // TODO: Check that value is within bounds. (VariableDescription
        // does not currently have bounds information.)
    }


    // Returns vacuously if the specified connection is valid, otherwise
    // throws an exception with an explanatory message.
    // The slave name parameters are only used for error messages.
    void EnforceValidConnection(
        const std::string& sourceSlaveName,
        const coral::model::VariableDescription& sourceVariable,
        const std::string& targetSlaveName,
        const coral::model::VariableDescription& targetVariable)
    {
        // Check causality
        if (sourceVariable.Causality() == coral::model::OUTPUT_CAUSALITY) {
            if (targetVariable.Causality() != coral::model::INPUT_CAUSALITY) {
                throw ModelConstructionException{
                    ConnectionErrMsg(
                        sourceSlaveName, sourceVariable.Name(),
                        targetSlaveName, targetVariable.Name(),
                        "An output variable may only be connected to an input variable")};
            }
        } else if (sourceVariable.Causality() == coral::model::CALCULATED_PARAMETER_CAUSALITY) {
            if (targetVariable.Causality() != coral::model::PARAMETER_CAUSALITY &&
                targetVariable.Causality() != coral::model::INPUT_CAUSALITY) {
                throw ModelConstructionException{
                    ConnectionErrMsg(
                        sourceSlaveName, sourceVariable.Name(),
                        targetSlaveName, targetVariable.Name(),
                        "A calculated parameter may only be connected to a parameter or input variable")};
            }
        } else {
            throw ModelConstructionException{
                ConnectionErrMsg(
                    sourceSlaveName, sourceVariable.Name(),
                    targetSlaveName, targetVariable.Name(),
                    "Only output variables and calculated parameters may be used as sources in a connection")};
        }

        // Check data type
        if (sourceVariable.DataType() != targetVariable.DataType()) {
            throw ModelConstructionException{
                ConnectionErrMsg(
                    sourceSlaveName, sourceVariable.Name(),
                    targetSlaveName, targetVariable.Name(),
                    "A variable of type "
                        + coral::model::DataTypeName(sourceVariable.DataType())
                        + " cannot be connected to a variable of type "
                        + coral::model::DataTypeName(targetVariable.DataType()))};
        }
    }
}


class ModelBuilder::Impl
{
public:
    void AddSlave(
        const std::string& name,
        const coral::model::SlaveTypeDescription& type)
    {
        if (!coral::model::IsValidSlaveName(name)) {
            throw std::invalid_argument{"Not a valid slave name: " + name};
        }
        if (m_slaves.count(name)) {
            throw ModelConstructionException{"Slave name already in use: " + name};
        }
        const auto cachedType =
            m_slaveTypes
            .insert(std::make_pair(type.UUID(), CachedSlaveType{type}))
            .first;
        m_slaves.insert(std::make_pair(name, &(cachedType->second)));
    }

    void SetInitialValue(
        const QualifiedVariableName& variable,
        const coral::model::ScalarValue& value)
    {
        const auto& varDesc = GetVariableDescription(variable);
        EnforceValidValue(variable.Slave(), varDesc, value);
        m_initialValues[variable] = value;
    }

    const coral::model::ScalarValue& GetInitialValue(
        const QualifiedVariableName& variable)
        const
    {
        auto valueIt = m_initialValues.find(variable);
        if (valueIt == m_initialValues.end()) {
            // TODO: Return the default initial value for the variable.
            // (Currently not carried by coral::model::VariableDescription.)
            throw EntityNotFoundException{
                "No initial value set for variable " + variable.ToString()};
        }
        return valueIt->second;
    }
    void ResetInitialValue(const QualifiedVariableName& variable)
    {
        m_initialValues.erase(variable);
    }

    void ConnectVariables(
        const QualifiedVariableName& source,
        const QualifiedVariableName& target)
    {
        EnforceValidConnection(
            source.Slave(), GetVariableDescription(source),
            target.Slave(), GetVariableDescription(target));

        auto ins = m_connections.insert(std::make_pair(source, target));
        if (!ins.second) {
            throw ModelConstructionException{
                "Variable already connected: " + target.ToString()};
        }
    }

private:
    const coral::model::VariableDescription& GetVariableDescription(
        const QualifiedVariableName& variable)
    {
        const auto slaveTypeIt = m_slaves.find(variable.Slave());
        if (slaveTypeIt == m_slaves.end()) {
            throw EntityNotFoundException{
                "Unknown slave name: " + variable.Slave()};
        }
        const auto& slaveType = *(slaveTypeIt->second);

        const auto variableIt = slaveType.variables.find(variable.Variable());
        if (variableIt == slaveType.variables.end()) {
            throw EntityNotFoundException{
                "Unknown variable: " + variable.ToString()};
        }
        return *(variableIt->second);
    }

    std::unordered_map<std::string, CachedSlaveType> m_slaveTypes;
    std::unordered_map<std::string, const CachedSlaveType*> m_slaves;
    std::unordered_map<QualifiedVariableName, coral::model::ScalarValue>
        m_initialValues;
    std::unordered_map<QualifiedVariableName, QualifiedVariableName>
        m_connections;
};


ModelBuilder::ModelBuilder() noexcept
    : m_impl(new Impl{}, [] (Impl* impl) { delete impl; })
{
}


void ModelBuilder::AddSlave(
    const std::string& name,
    const coral::model::SlaveTypeDescription& type)
{
    m_impl->AddSlave(name, type);
}


void ModelBuilder::SetInitialValue(
    const QualifiedVariableName& variable,
    const coral::model::ScalarValue& value)
{
    m_impl->SetInitialValue(variable, value);
}


const coral::model::ScalarValue& ModelBuilder::GetInitialValue(
    const QualifiedVariableName& variable)
    const
{
    return m_impl->GetInitialValue(variable);
}


void ModelBuilder::ResetInitialValue(const QualifiedVariableName& variable)
{
    m_impl->ResetInitialValue(variable);
}


void ModelBuilder::ConnectVariables(
    const QualifiedVariableName& source,
    const QualifiedVariableName& target)
{
    m_impl->ConnectVariables(source, target);
}


}} // namespace
