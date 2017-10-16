#include <vector>
#include <gtest/gtest.h>
#include <coral/master/model_builder.hpp>


TEST(coral_master, QualifiedVariableName)
{
    using namespace coral::master;
    auto a = QualifiedVariableName{"slaveA", "var1"};
    EXPECT_EQ("slaveA", a.Slave());
    EXPECT_EQ("var1", a.Variable());
    EXPECT_EQ("slaveA.var1", a.ToString());
    EXPECT_TRUE(a == QualifiedVariableName("slaveA", "var1"));
    EXPECT_FALSE(a != QualifiedVariableName("slaveA", "var1"));

    auto b = QualifiedVariableName::FromString("slaveB.var1");
    EXPECT_EQ("slaveB", b.Slave());
    EXPECT_EQ("var1", b.Variable());
    EXPECT_EQ("slaveB.var1", b.ToString());
    EXPECT_FALSE(b == a);
    EXPECT_TRUE(b != a);

    const auto h = std::hash<QualifiedVariableName>{};
    EXPECT_NE(h(a), h(b));
    
    auto c = QualifiedVariableName{"slaveA", "var2"};
    EXPECT_FALSE(c == a);
    EXPECT_TRUE(c != a);
    EXPECT_NE(h(c), h(a));
    EXPECT_FALSE(c == b);
    EXPECT_TRUE(c != b);
    EXPECT_NE(h(c), h(b));
}


TEST(coral_master, ModelBuilder)
{
    using namespace coral::master;
    using namespace coral::model;
    const auto slaveType1 = SlaveTypeDescription(
        "widget",
        "b331f8fc-3958-45ad-92fc-e88e57df4297",
        "A widget that does something",
        "A. Widgetmaker",
        "1.0",
        std::vector<VariableDescription>{
            VariableDescription{0, "a", REAL_DATATYPE,   OUTPUT_CAUSALITY, CONTINUOUS_VARIABILITY},
            VariableDescription{1, "b", REAL_DATATYPE,   OUTPUT_CAUSALITY, FIXED_VARIABILITY},
            VariableDescription{2, "c", STRING_DATATYPE, OUTPUT_CAUSALITY, DISCRETE_VARIABILITY}
        });
    const auto slaveType2 = SlaveTypeDescription(
        "gadget",
        "8876b42f-db2b-4b84-8695-1752057d3562",
        "An interesting gadget",
        "Gadgets Gadgets Gadgets",
        "3.4",
        std::vector<VariableDescription>{
            VariableDescription{10, "x", REAL_DATATYPE,   INPUT_CAUSALITY, CONTINUOUS_VARIABILITY},
            VariableDescription{20, "y", REAL_DATATYPE,   INPUT_CAUSALITY, CONTINUOUS_VARIABILITY},
            VariableDescription{30, "z", STRING_DATATYPE, INPUT_CAUSALITY, FIXED_VARIABILITY}
        });

    auto mb = ModelBuilder{};

    mb.AddSlave("slave1", slaveType1);
    mb.AddSlave("slave2", slaveType2);
    EXPECT_THROW(mb.AddSlave("slave2", slaveType1), ModelConstructionException);

    mb.SetInitialValue(QualifiedVariableName{"slave2", "x"}, 4.0);
    mb.SetInitialValue(QualifiedVariableName{"slave2", "z"}, std::string("foo"));
    EXPECT_THROW(
        mb.SetInitialValue(QualifiedVariableName{"slave2", "x"}, 123),
        ModelConstructionException);
    EXPECT_THROW(
        mb.SetInitialValue(QualifiedVariableName{"slave2", "x"}, true),
        ModelConstructionException);
    EXPECT_THROW(
        mb.SetInitialValue(QualifiedVariableName{"slave2", "x"}, std::string("foo")),
        ModelConstructionException);
    EXPECT_THROW(
        mb.SetInitialValue(QualifiedVariableName{"slave3", "x"}, 0),
        EntityNotFoundException);
    EXPECT_THROW(
        mb.SetInitialValue(QualifiedVariableName{"slave2", "e"}, 0),
        EntityNotFoundException);

    EXPECT_EQ(4.0, boost::get<double>(mb.GetInitialValue(QualifiedVariableName{"slave2", "x"})));
    EXPECT_EQ("foo", boost::get<std::string>(mb.GetInitialValue(QualifiedVariableName{"slave2", "z"})));
    EXPECT_THROW(
        mb.GetInitialValue(QualifiedVariableName{"slave3", "x"}),
        EntityNotFoundException);
    EXPECT_THROW(
        mb.GetInitialValue(QualifiedVariableName{"slave2", "e"}),
        EntityNotFoundException);

}
