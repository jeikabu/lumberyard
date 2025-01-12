/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#include <EMotionFX/Source/AnimGraph.h>
#include <EMotionFX/Source/AnimGraphBindPoseNode.h>
#include <EMotionFX/Source/AnimGraphStateMachine.h>
#include <EMotionFX/Source/BlendTree.h>
#include <EMotionFX/Source/BlendTreeParameterNode.h>
#include <EMotionFX/Source/BlendTreeTwoLinkIKNode.h>
#include <EMotionFX/Source/EMotionFXManager.h>
#include <EMotionFX/Source/Parameter/RotationParameter.h>
#include <MCore/Source/ReflectionSerializer.h>
#include <Tests/AnimGraphFixture.h>

namespace EMotionFX
{
    class QuaternionParameterFixture
        : public AnimGraphFixture
        , public ::testing::WithParamInterface<AZ::Quaternion>
    {
    public:
        void ConstructGraph() override
        {
            AnimGraphFixture::ConstructGraph();
            m_param = GetParam();
            AddParameter<RotationParameter, AZ::Quaternion>("quaternionTest", m_param);

            m_blendTree = aznew BlendTree();
            m_animGraph->GetRootStateMachine()->AddChildNode(m_blendTree);
            m_animGraph->GetRootStateMachine()->SetEntryState(m_blendTree);

            /*
            +------------+
            |bindPoseNode+---+
            +------------+   |
                             +-->+-------------+     +---------+
             +-----------+       |twoLinkIKNode+---->+finalNode|
             |m_paramNode+------>+-------------+     +---------+
             +-----------+
            */
            BlendTreeFinalNode* finalNode = aznew BlendTreeFinalNode();
            AnimGraphBindPoseNode* bindPoseNode = aznew AnimGraphBindPoseNode();
            m_paramNode = aznew BlendTreeParameterNode();

            // Using two link IK Node because its GoalRot input port uses quaternion.
            m_twoLinkIKNode = aznew BlendTreeTwoLinkIKNode();

            m_blendTree->AddChildNode(finalNode);
            m_blendTree->AddChildNode(m_twoLinkIKNode);
            m_blendTree->AddChildNode(bindPoseNode);
            m_blendTree->AddChildNode(m_paramNode);

            m_twoLinkIKNode->AddConnection(bindPoseNode, AnimGraphBindPoseNode::PORTID_OUTPUT_POSE, BlendTreeTwoLinkIKNode::PORTID_INPUT_POSE);
            finalNode->AddConnection(m_twoLinkIKNode, BlendTreeTwoLinkIKNode::PORTID_OUTPUT_POSE, BlendTreeFinalNode::PORTID_INPUT_POSE);
        };

        template <class paramType, class inputType>
        void ParamSetValue(const AZStd::string& paramName, const inputType& value)
        {
            const AZ::Outcome<size_t> parameterIndex = m_animGraphInstance->FindParameterIndex(paramName);
            MCore::Attribute* param = m_animGraphInstance->GetParameterValue(static_cast<AZ::u32>(parameterIndex.GetValue()));
            paramType* typeParam = static_cast<paramType*>(param);
            typeParam->SetValue(value);
        };

    protected:
        AZ::Quaternion m_param;
        BlendTree* m_blendTree = nullptr;
        BlendTreeParameterNode* m_paramNode = nullptr;
        BlendTreeTwoLinkIKNode* m_twoLinkIKNode = nullptr;

    private:
        template<class ParameterType, class ValueType>
        void AddParameter(const AZStd::string& name, const ValueType& defaultValue)
        {
            ParameterType* parameter = aznew ParameterType();
            parameter->SetName(name);
            parameter->SetDefaultValue(defaultValue);
            m_animGraph->AddParameter(parameter);
        }
    };

    TEST_P(QuaternionParameterFixture, ParameterOutputsCorrectQuaternion)
    {
        // Parameter node needs to connect to another node, otherwise it will not update.
        m_twoLinkIKNode->AddConnection(m_paramNode, m_paramNode->FindOutputPortIndex("quaternionTest"), BlendTreeTwoLinkIKNode::PORTID_INPUT_GOALROT);
        GetEMotionFX().Update(1.0f / 60.0f);

        // Check correct output for quaternion parameter.
        const MCore::Quaternion& quaternionTestParam = m_paramNode->GetOutputQuaternion(m_animGraphInstance,
            m_paramNode->FindOutputPortIndex("quaternionTest"))->GetValue();

        EXPECT_TRUE(quaternionTestParam.x == m_param.GetX()) << "Quaternion X value should be the same as expected Quaternion X value.";
        EXPECT_TRUE(quaternionTestParam.y == m_param.GetY()) << "Quaternion Y value should be the same as expected Quaternion Y value.";
        EXPECT_TRUE(quaternionTestParam.z == m_param.GetZ()) << "Quaternion Z value should be the same as expected Quaternion Z value.";
        EXPECT_TRUE(quaternionTestParam.w == m_param.GetW()) << "Quaternion W value should be the same as expected Quaternion W value.";
    }

    TEST_P(QuaternionParameterFixture, QuaternionSetValueOutputsCorrectQuaternion)
    {
        m_twoLinkIKNode->AddConnection(m_paramNode, m_paramNode->FindOutputPortIndex("quaternionTest"), BlendTreeTwoLinkIKNode::PORTID_INPUT_GOALROT);
        GetEMotionFX().Update(1.0f / 60.0f);

        // Shuffle the Quaternion parameter values to check changing quaternion values will be processed correctly.
        ParamSetValue<MCore::AttributeQuaternion, MCore::Quaternion>("quaternionTest", MCore::Quaternion(m_param.GetY(), m_param.GetZ(), m_param.GetX(), m_param.GetW()));
        GetEMotionFX().Update(1.0f / 60.0f);

        const MCore::Quaternion& quaternionTestParam = m_paramNode->GetOutputQuaternion(m_animGraphInstance,
            m_paramNode->FindOutputPortIndex("quaternionTest"))->GetValue();
        EXPECT_FLOAT_EQ(quaternionTestParam.x, m_param.GetY()) << "Input Quaternion X value should be the same as expected Quaternion Y value.";
        EXPECT_FLOAT_EQ(quaternionTestParam.y, m_param.GetZ()) << "Input Quaternion Y value should be the same as expected Quaternion Z value.";
        EXPECT_FLOAT_EQ(quaternionTestParam.z, m_param.GetX()) << "Input Quaternion Z value should be the same as expected Quaternion X value.";
        EXPECT_FLOAT_EQ(quaternionTestParam.w, m_param.GetW()) << "Input Quaternion W value should be the same as expected Quaternion W value.";
    };

    std::vector<AZ::Quaternion> quaternionParameterTestData
    {
        AZ::Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
        AZ::Quaternion(1.0f, 0.5f, -0.5f, 1.0f),
        AZ::Quaternion(AZ::g_fltMax, -AZ::g_fltMax, AZ::g_fltEps, 1.0f)
    };

    INSTANTIATE_TEST_CASE_P(QuaternionParameter_ValidOutputTests,
        QuaternionParameterFixture,
        ::testing::ValuesIn(quaternionParameterTestData)
    );
} // end namespace EMotionFX