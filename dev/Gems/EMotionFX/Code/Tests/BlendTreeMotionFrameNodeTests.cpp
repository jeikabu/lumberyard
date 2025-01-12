
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

#include <Tests/AnimGraphFixture.h>
#include <EMotionFX/Source/AnimGraph.h>
#include <EMotionFX/Source/AnimGraphMotionNode.h>
#include <EMotionFX/Source/AnimGraphStateMachine.h>
#include <EMotionFX/Source/BlendTree.h>
#include <EMotionFX/Source/BlendTreeMotionFrameNode.h>
#include <EMotionFX/Source/Motion.h>
#include <EMotionFX/Source/MotionSet.h>
#include <EMotionFX/Source/SkeletalMotion.h>

namespace EMotionFX
{
    class BlendTreeMotionFrameTests
        : public AnimGraphFixture
    {
    public:
        void ConstructGraph() override
        {
            AnimGraphFixture::ConstructGraph();

            /*
            +--------+    +--------------+    +------------+
            | Motion +--->+ Motion Frame +--->+ Final Node |
            +--------+    +--------------+    +------------+
            */
            m_blendTree = aznew BlendTree();
            m_animGraph->GetRootStateMachine()->AddChildNode(m_blendTree);
            m_animGraph->GetRootStateMachine()->SetEntryState(m_blendTree);

            m_motionFrameNode = aznew BlendTreeMotionFrameNode();
            m_blendTree->AddChildNode(m_motionFrameNode);
            BlendTreeFinalNode* finalNode = aznew BlendTreeFinalNode();
            m_blendTree->AddChildNode(finalNode);
            finalNode->AddConnection(m_motionFrameNode, BlendTreeMotionFrameNode::OUTPUTPORT_RESULT, BlendTreeFinalNode::PORTID_INPUT_POSE);

            m_motionNode = aznew AnimGraphMotionNode();
            m_blendTree->AddChildNode(m_motionNode);
            m_motionFrameNode->AddConnection(m_motionNode, AnimGraphMotionNode::PORTID_OUTPUT_MOTION, BlendTreeMotionFrameNode::INPUTPORT_MOTION);
        }

        void SetUp() override
        {
            AnimGraphFixture::SetUp();

            const char* motionId = "Test Motion";
            SkeletalMotion* motion = SkeletalMotion::Create(motionId);
            motion->SetMaxTime(m_motionDuration);
            MotionSet::MotionEntry* motionEntry = aznew MotionSet::MotionEntry(motionId, motionId, motion);
            m_motionSet->AddMotionEntry(motionEntry);
            m_motionNode->AddMotionId(motionId);

            m_motionNode->RecursiveOnChangeMotionSet(m_animGraphInstance, m_motionSet);
            m_motionNode->PickNewActiveMotion(m_animGraphInstance, static_cast<AnimGraphMotionNode::UniqueData*>(m_motionNode->FindUniqueNodeData(m_animGraphInstance)));

            // The motion node creates its motion instance in the Output() call, which means that at the first
            // evaluation of the motion frame node the duration is 0.0 (due to the missing motion instance) and
            // that results in the normalized time also being 0.0 at that frame which we need for testing.
            GetEMotionFX().Update(0.0f);
        }

        void SetAndTestTimeValue(BlendTreeMotionFrameNode::UniqueData* uniqueData, float newNormalizedTime, bool rewind = false)
        {
            const float prevNewTime = uniqueData->mNewTime;
            m_motionFrameNode->SetNormalizedTimeValue(newNormalizedTime);

            if (rewind)
            {
                m_motionFrameNode->Rewind(m_animGraphInstance);
            }
            GetEMotionFX().Update(0.0f);

            EXPECT_EQ(m_motionFrameNode->GetNormalizedTimeValue(), newNormalizedTime);
            
            float expectedOldTime = prevNewTime;
            if (rewind)
            {
                if (m_motionFrameNode->GetEmitEventsFromStart())
                {
                    expectedOldTime = 0.0f;
                }
                else
                {
                    expectedOldTime = newNormalizedTime * m_motionDuration;
                }
            }
            EXPECT_EQ(uniqueData->mOldTime, expectedOldTime);
            
            EXPECT_EQ(uniqueData->mNewTime, newNormalizedTime * m_motionDuration);
        }

    public:
        float m_motionDuration = 1.0f;
        AnimGraphMotionNode* m_motionNode = nullptr;
        BlendTreeMotionFrameNode* m_motionFrameNode = nullptr;
        BlendTreeMotionFrameNode::UniqueData* m_motionFrameNodeUniqueData = nullptr;
        BlendTree* m_blendTree = nullptr;
    };

    TEST_F(BlendTreeMotionFrameTests, SetNormalizedTime)
    {
        BlendTreeMotionFrameNode::UniqueData* uniqueData = static_cast<BlendTreeMotionFrameNode::UniqueData*>(m_motionFrameNode->FindUniqueNodeData(m_animGraphInstance));
        SetAndTestTimeValue(uniqueData, 0.2f);
        SetAndTestTimeValue(uniqueData, 0.4f);
        SetAndTestTimeValue(uniqueData, 0.3f);
        SetAndTestTimeValue(uniqueData, 1.0f);
        SetAndTestTimeValue(uniqueData, 0.0f);
    }

    TEST_F(BlendTreeMotionFrameTests, RewindTest)
    {
        BlendTreeMotionFrameNode::UniqueData* uniqueData = static_cast<BlendTreeMotionFrameNode::UniqueData*>(m_motionFrameNode->FindUniqueNodeData(m_animGraphInstance));
        SetAndTestTimeValue(uniqueData, 0.2f);
        SetAndTestTimeValue(uniqueData, 0.4f, true);
    }

    TEST_F(BlendTreeMotionFrameTests, RewindTest_SetEmitEventsFromStart)
    {
        BlendTreeMotionFrameNode::UniqueData* uniqueData = static_cast<BlendTreeMotionFrameNode::UniqueData*>(m_motionFrameNode->FindUniqueNodeData(m_animGraphInstance));
        m_motionFrameNode->SetEmitEventsFromStart(true);
        SetAndTestTimeValue(uniqueData, 0.2f);
        SetAndTestTimeValue(uniqueData, 0.4f, true);
    }
} // namespace EMotionFX
